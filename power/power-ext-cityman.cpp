/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Cityman (MSM8994) extension of the QTI power HAL
 * (vendor/qcom/opensource/power, android.hardware.power-service-qti).
 *
 * The HAL sends interaction and launch boosts through perf_hint(), which the
 * MSM8994 perfd client library does not export, so without this extension
 * every hint is a no-op. Here the hints are turned into perf locks
 * (perf_lock_acq through interaction_with_handle()) with the 16-bit
 * "0xRRVV" resource opcodes of the MSM8994 perfd: RR is the resource, VV the
 * value; for CPU frequencies VV is in units of 100 MHz and perfd picks the
 * nearest frequency of the cluster table.
 *
 * Linking: the HAL links this file as a static library selected by the soong
 * config variable qtipower/mode_ext_lib. The linker pulls an archive member
 * only for undefined symbols: setDeviceSpecificMode() and
 * isDeviceSpecificModeSupported() are the undefined ones, so the strong
 * power_hint_override() and set_interactive_override() below must stay in
 * this same translation unit to replace the weak defaults of power-common.c.
 */

#define LOG_TAG "QTI PowerHAL cityman"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <mutex>

#include <aidl/android/hardware/power/Mode.h>
#include <hardware/power.h>
#include <log/log.h>

#include "power-common.h"
#include "utils.h"

namespace {

/* perfd resource opcodes (MSM8994, old mpctl encoding). */
constexpr int kLittleMinFreq1248 = 0x20C;  /* CPU0 (A53 cluster) min >= 1.2 GHz */
constexpr int kLittleMinFreq1344 = 0x20D;  /* CPU0 (A53 cluster) min >= 1.3 GHz */
constexpr int kLittleMaxFreq864 = 0x1509;  /* CPU0 (A53 cluster) max ~0.9 GHz */
constexpr int kBigMinFreq864 = 0x1F08;     /* CPU4 (A57 cluster) min >= 0.8 GHz */
constexpr int kBigMinFreq1248 = 0x1F0C;    /* CPU4 (A57 cluster) min >= 1.2 GHz */
constexpr int kBigMaxFreq1248 = 0x230C;    /* CPU4 (A57 cluster) max ~1.2 GHz */
constexpr int kSchedBoostOn = 0x1E01;      /* /proc/sys/kernel/sched_boost = 1 */
constexpr int kSchedUpmigrate = 0x4E00;    /* | percent: sched_upmigrate */
constexpr int kSchedDownmigrate = 0x4F00;  /* | percent: sched_downmigrate */

/*
 * HMP default written by init.cityman.power.sh (downmigrate 85). The kernel
 * rejects a sched_downmigrate above sched_upmigrate, so the migration locks
 * are taken and released one at a time, in an order that is valid from any
 * consistent default pair: when a lock lowers the values, upmigrate is
 * restored first; when it raises them (screen off), downmigrate first.
 */
constexpr int kDefaultUpmigrate = 95;
constexpr int kScreenOffUpmigrate = 99;
constexpr int kScreenOffDownmigrate = 95;

/* Interaction boost, as on angler. */
constexpr int kLittleBoostMs = 1500;
constexpr int kBigBoostMs = 500;
constexpr int kDownmigrateMs = 1000;
constexpr int kUpmigrateMs = 500;
constexpr int64_t kMaxElapsedUs = 750000;
constexpr int64_t kMinBoostIntervalUs = 250000;
constexpr int kFlingDurationMs = 750;
constexpr int kMaxHintDurationMs = 5000;
constexpr int kFlingUpmigrate = 20;

/* Launch boost; released early when the framework ends the launch. */
constexpr int kLaunchBoostMs = 1500;

enum SustainedBits : unsigned {
    SUSTAINED_BIT = 1u << 0,
    FIXED_BIT = 1u << 1,
};

struct TimedLock {
    int handle = 0;
    int64_t expiry_us = 0; /* INT64_MAX for locks without timeout */
};

std::mutex g_lock;
TimedLock g_little;
TimedLock g_big;
TimedLock g_upmigrate;
TimedLock g_downmigrate;
TimedLock g_launch;
TimedLock g_sustained;
TimedLock g_display_up;
TimedLock g_display_down;
unsigned g_sustained_mask = 0;
bool g_display_on = true;
int64_t g_previous_boost_us = 0;

template <typename T, size_t N>
constexpr int count_of(T (&)[N]) {
    return static_cast<int>(N);
}

int64_t now_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

bool lock_active(const TimedLock& lock) {
    return CHECK_HANDLE(lock.handle) && now_us() < lock.expiry_us;
}

/*
 * Release a lock only while perfd still holds it: once a timed lock expires
 * perfd drops it by itself and the handle number may be reused by another
 * client (thermal-engine also takes perf locks).
 */
void lock_release(TimedLock& lock) {
    if (lock_active(lock)) {
        release_request(lock.handle);
    }
    lock.handle = 0;
    lock.expiry_us = 0;
}

/* duration_ms == 0 keeps the lock until it is released. */
void lock_acquire(TimedLock& lock, int duration_ms, int* resources, int count) {
    lock_release(lock);
    int handle = interaction_with_handle(0, duration_ms, count, resources);
    if (!CHECK_HANDLE(handle)) {
        ALOGE("perf lock 0x%X (%d resources, %d ms) failed: %d", resources[0], count,
              duration_ms, handle);
        return;
    }
    lock.handle = handle;
    lock.expiry_us = duration_ms > 0 ? now_us() + static_cast<int64_t>(duration_ms) * 1000
                                     : INT64_MAX;
}

/* These locks lower the values: upmigrate goes back to its default first. */
void release_migrate_locks() {
    lock_release(g_upmigrate);
    lock_release(g_downmigrate);
}

void release_boost_locks() {
    lock_release(g_little);
    lock_release(g_big);
    release_migrate_locks();
    lock_release(g_launch);
}

int process_interaction(void* data) {
    int duration_hint = data ? *static_cast<int*>(data) : 0;

    std::lock_guard<std::mutex> guard(g_lock);

    /* No touch boost in sustained performance mode or with the screen off. */
    if (g_sustained_mask || !g_display_on) return HINT_HANDLED;

    int64_t now = now_us();
    int64_t elapsed = now - g_previous_boost_us;
    if (elapsed > kMaxElapsedUs) {
        elapsed = kMaxElapsedUs;
    } else if (elapsed < kMinBoostIntervalUs && duration_hint <= kFlingDurationMs) {
        /* Less than 250 ms since the last boost and not a fling. */
        return HINT_HANDLED;
    }
    g_previous_boost_us = now;

    /*
     * sched_upmigrate: 95 for touches in quick succession down to 20 for a
     * sporadic touch (750 ms or more since the last one), 20 when flinging.
     * sched_downmigrate is half of it.
     */
    double ratio = static_cast<double>(elapsed) * static_cast<double>(elapsed) /
                   (static_cast<double>(kMaxElapsedUs) * static_cast<double>(kMaxElapsedUs));
    int upmigrate = kDefaultUpmigrate - static_cast<int>(75.0 * ratio);
    if (duration_hint >= kFlingDurationMs) upmigrate = kFlingUpmigrate;
    int downmigrate = upmigrate / 2;

    int little_ms = kLittleBoostMs;
    int downmigrate_ms = kDownmigrateMs;
    if (duration_hint > kDownmigrateMs) {
        downmigrate_ms = duration_hint < kMaxHintDurationMs ? duration_hint : kMaxHintDurationMs;
        /* Keep the A53s up past the downmigrate window to soften it. */
        little_ms = downmigrate_ms + 750;
    }

    int little[] = {kLittleMinFreq1248};
    int big[] = {kBigMinFreq864};
    int down[] = {kSchedDownmigrate | downmigrate};
    int up[] = {kSchedUpmigrate | upmigrate};

    lock_acquire(g_little, little_ms, little, count_of(little));
    lock_acquire(g_big, kBigBoostMs, big, count_of(big));
    /*
     * Back to 95/85 first, then downmigrate (<= 47 <= 95) and upmigrate
     * (>= downmigrate). The downmigrate lock always outlives the upmigrate
     * one, so the expiries restore 95 before 85 as well.
     */
    release_migrate_locks();
    lock_acquire(g_downmigrate, downmigrate_ms, down, count_of(down));
    lock_acquire(g_upmigrate, kUpmigrateMs, up, count_of(up));

    ALOGD("interaction: hint %d ms, little %d ms, upmigrate %d/%d for %d/%d ms", duration_hint,
          little_ms, upmigrate, downmigrate, kUpmigrateMs, downmigrate_ms);
    return HINT_HANDLED;
}

int process_launch(void* data) {
    std::lock_guard<std::mutex> guard(g_lock);

    if (!data) {
        /* Launch finished: drop the boost early. */
        lock_release(g_launch);
        return HINT_HANDLED;
    }

    if (g_sustained_mask || lock_active(g_launch)) return HINT_HANDLED;

    int resources[] = {kSchedBoostOn, kLittleMinFreq1344, kBigMinFreq1248};
    lock_acquire(g_launch, kLaunchBoostMs, resources, count_of(resources));
    ALOGI("launch boost for %d ms (handle %d)", kLaunchBoostMs, g_launch.handle);
    return HINT_HANDLED;
}

void set_sustained(unsigned bit, bool enabled) {
    std::lock_guard<std::mutex> guard(g_lock);

    unsigned mask = enabled ? (g_sustained_mask | bit) : (g_sustained_mask & ~bit);
    if (mask == g_sustained_mask) return;
    g_sustained_mask = mask;

    if (mask && !CHECK_HANDLE(g_sustained.handle)) {
        /* Hold both clusters at a level they can keep without throttling. */
        release_boost_locks();
        int resources[] = {kLittleMaxFreq864, kBigMaxFreq1248};
        lock_acquire(g_sustained, 0, resources, count_of(resources));
        ALOGI("sustained performance on (mask 0x%x, handle %d)", mask, g_sustained.handle);
    } else if (!mask) {
        lock_release(g_sustained);
        ALOGI("sustained performance off");
    }
}

}  // namespace

extern "C" int power_hint_override(power_hint_t hint, void* data) {
    switch (hint) {
        case POWER_HINT_INTERACTION:
            return process_interaction(data);
        case POWER_HINT_LAUNCH:
            return process_launch(data);
        default:
            break;
    }
    return HINT_NONE;
}

extern "C" int set_interactive_override(int on) {
    std::lock_guard<std::mutex> guard(g_lock);

    if (!on) {
        g_display_on = false;
        release_boost_locks();
        if (!CHECK_HANDLE(g_display_up.handle)) {
            /*
             * Screen off: sched_upmigrate 99, sched_downmigrate 95, so the
             * A57s stay available but are very hard to reach. Raising:
             * upmigrate first, then downmigrate.
             */
            int up[] = {kSchedUpmigrate | kScreenOffUpmigrate};
            int down[] = {kSchedDownmigrate | kScreenOffDownmigrate};
            lock_acquire(g_display_up, 0, up, count_of(up));
            lock_acquire(g_display_down, 0, down, count_of(down));
        }
    } else {
        g_display_on = true;
        /* Lowering back: downmigrate first, then upmigrate. */
        lock_release(g_display_down);
        lock_release(g_display_up);
    }
    ALOGI("display %s", on ? "on" : "off");
    return HINT_HANDLED;
}

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace impl {

bool isDeviceSpecificModeSupported(Mode /* type */, bool* /* _aidl_return */) {
    /* LAUNCH, INTERACTIVE, SUSTAINED and FIXED_PERFORMANCE are already reported. */
    return false;
}

bool setDeviceSpecificMode(Mode type, bool enabled) {
    switch (type) {
        /*
         * Power::setMode() passes no enabled flag to power_hint() for these
         * modes, so they are handled here, where it is known.
         */
        case Mode::SUSTAINED_PERFORMANCE:
            set_sustained(SUSTAINED_BIT, enabled);
            return true;
        case Mode::FIXED_PERFORMANCE:
            set_sustained(FIXED_BIT, enabled);
            return true;
        default:
            return false;
    }
}

}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
