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
 * next frequency of the cluster table.
 *
 * It also holds the performance profile (power save, balanced,
 * performance) as a perf lock that lasts until the profile changes; see
 * power/README.md.
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
#include <stdio.h>
#include <sys/system_properties.h>
#include <time.h>

#include <initializer_list>
#include <mutex>
#include <thread>

#include <aidl/android/hardware/power/Mode.h>
#include <cutils/properties.h>
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
 * Online core limit of the A57 cluster. perfd applies it through core_ctl
 * (cpu4/core_ctl/max_cpus = 0xFF - VV) and on release writes back
 * ro.core_ctl_max_cpu, while min_cpus returns to its previous value.
 */
constexpr int kBigCoresMax0 = 0x3DFF;      /* A57 cluster offline */
constexpr int kBigCoresMax2 = 0x3DFD;      /* at most two A57 online */
constexpr int kLittleMaxFreq1248 = 0x150C; /* CPU0 (A53 cluster) max 1248 MHz */
constexpr int kBigMaxFreq1440 = 0x230E;    /* CPU4 (A57 cluster) max 1440 MHz */

/*
 * Performance profiles, numbered as the LineageOS power HAL profiles. The
 * profile comes from persist.vendor.power.profile (CitymanParts, in Settings
 * > Battery and Quick Settings); Battery Saver (Mode::LOW_POWER) forces
 * power save while it is on.
 */
enum Profile : int {
    PROFILE_POWER_SAVE = 0,
    PROFILE_BALANCED = 1,
    PROFILE_HIGH_PERFORMANCE = 2,
};
constexpr int kDefaultProfile = PROFILE_BALANCED;
constexpr char kProfileProp[] = "persist.vendor.power.profile";
constexpr char kActiveProfileProp[] = "vendor.power.profile.active";
/* Set by init when perfd starts or restarts: perf locks die with perfd. */
constexpr char kPerfdProp[] = "vendor.power.perfd";
/* perfd needs a moment after start before it accepts requests. */
constexpr int kProfileRetries = 20;
constexpr long kProfileRetryNs = 500000000L;

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
TimedLock g_profile;
unsigned g_sustained_mask = 0;
int g_user_profile = kDefaultProfile;
bool g_low_power = false;
int g_applied_profile = -1; /* -1: profile not applied (perfd not reached) */
bool g_perfd_known = false; /* init has started perfd at least once */
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

/* Effective profile: Battery Saver wins over the user choice. */
int current_profile() {
    return g_low_power ? PROFILE_POWER_SAVE : g_user_profile;
}

const char* profile_name(int profile) {
    switch (profile) {
        case PROFILE_POWER_SAVE:
            return "power save";
        case PROFILE_BALANCED:
            return "balanced";
        case PROFILE_HIGH_PERFORMANCE:
            return "performance";
        default:
            return "unknown";
    }
}

int read_user_profile() {
    int profile = property_get_int32(kProfileProp, kDefaultProfile);
    if (profile < PROFILE_POWER_SAVE || profile > PROFILE_HIGH_PERFORMANCE) {
        ALOGW("%s=%d is not a profile, using balanced", kProfileProp, profile);
        profile = kDefaultProfile;
    }
    return profile;
}

/*
 * Hold the perf lock of the effective profile. Returns false when perfd did
 * not take it (not started yet, or still starting): the caller retries.
 */
bool apply_profile() {
    int profile = current_profile();
    if (profile == g_applied_profile) return true;
    if (!g_perfd_known) return false;

    lock_release(g_profile);
    g_applied_profile = -1;

    if (profile == PROFILE_POWER_SAVE) {
        /* The A57s go offline: drop the boosts that use them. */
        release_boost_locks();
        int resources[] = {kBigCoresMax0, kLittleMaxFreq1248};
        lock_acquire(g_profile, 0, resources, count_of(resources));
    } else if (profile == PROFILE_BALANCED) {
        int resources[] = {kBigCoresMax2, kBigMaxFreq1440};
        lock_acquire(g_profile, 0, resources, count_of(resources));
    }
    /*
     * Performance holds no lock: the A57 maximum is the user maximum, which
     * belongs to thermal-engine (it throttles by writing scaling_max_freq).
     */
    if (profile != PROFILE_HIGH_PERFORMANCE && !CHECK_HANDLE(g_profile.handle)) return false;

    g_applied_profile = profile;
    char value[PROPERTY_VALUE_MAX];
    snprintf(value, sizeof(value), "%d", profile);
    property_set(kActiveProfileProp, value);
    ALOGI("profile %s%s (handle %d)", profile_name(profile),
          g_low_power ? " for Battery Saver" : "", g_profile.handle);
    return true;
}

void take_display_off_locks() {
    /*
     * Screen off: sched_upmigrate 99, sched_downmigrate 95, so the A57s stay
     * available but are very hard to reach. Raising: upmigrate first, then
     * downmigrate.
     */
    int up[] = {kSchedUpmigrate | kScreenOffUpmigrate};
    int down[] = {kSchedDownmigrate | kScreenOffDownmigrate};
    lock_acquire(g_display_up, 0, up, count_of(up));
    lock_acquire(g_display_down, 0, down, count_of(down));
}

void take_sustained_lock() {
    /* Hold both clusters at a level they can keep without throttling. */
    int resources[] = {kLittleMaxFreq864, kBigMaxFreq1248};
    lock_acquire(g_sustained, 0, resources, count_of(resources));
}

/*
 * perfd (re)started: the locks it held are gone and their handle numbers
 * may already belong to other clients, so forget them without releasing.
 */
void forget_locks() {
    for (TimedLock* lock : {&g_little, &g_big, &g_upmigrate, &g_downmigrate, &g_launch,
                            &g_sustained, &g_display_up, &g_display_down, &g_profile}) {
        lock->handle = 0;
        lock->expiry_us = 0;
    }
    g_applied_profile = -1;
}

/* Take back the locks that last until a state change. */
bool restore_held_locks() {
    bool ok = true;
    if (!g_display_on) {
        take_display_off_locks();
        ok = ok && CHECK_HANDLE(g_display_down.handle);
    }
    if (g_sustained_mask) {
        take_sustained_lock();
        ok = ok && CHECK_HANDLE(g_sustained.handle);
    }
    return ok;
}

uint32_t property_serial(const char* name) {
    const prop_info* info = __system_property_find(name);
    return info ? __system_property_serial(info) : 0;
}

/*
 * Follows the profile property and perfd restarts. Any property change wakes
 * the thread; it then compares the serials of the two it cares about.
 */
void profile_watcher() {
    uint32_t global_serial = 0;
    uint32_t profile_serial = 0;
    uint32_t perfd_serial = 0;
    bool profile_read = false;
    bool restore = false;
    int retries = 0;

    for (;;) {
        uint32_t new_profile_serial = property_serial(kProfileProp);
        uint32_t new_perfd_serial = property_serial(kPerfdProp);
        bool pending = false;
        {
            std::lock_guard<std::mutex> guard(g_lock);
            if (new_perfd_serial != perfd_serial) {
                perfd_serial = new_perfd_serial;
                ALOGI("perfd started, applying the profile");
                forget_locks();
                g_perfd_known = true;
                restore = true;
                retries = kProfileRetries;
            }
            if (!profile_read || new_profile_serial != profile_serial) {
                profile_serial = new_profile_serial;
                profile_read = true;
                g_user_profile = read_user_profile();
                retries = kProfileRetries;
            }
            bool ok = apply_profile();
            if (ok && restore) {
                ok = restore_held_locks();
                restore = !ok;
            }
            pending = !ok && g_perfd_known && retries-- > 0;
        }

        struct timespec retry = {0, kProfileRetryNs};
        uint32_t new_global_serial = global_serial;
        __system_property_wait(nullptr, global_serial, &new_global_serial,
                               pending ? &retry : nullptr);
        global_serial = new_global_serial;
    }
}

void start_profile_watcher() {
    static std::once_flag once;
    std::call_once(once, [] { std::thread(profile_watcher).detach(); });
}

void set_low_power(bool enabled) {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_low_power == enabled) return;
    g_low_power = enabled;
    ALOGI("Battery Saver %s", enabled ? "on" : "off");
    apply_profile();
}

int process_interaction(void* data) {
    int duration_hint = data ? *static_cast<int*>(data) : 0;

    start_profile_watcher();
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
    if (current_profile() == PROFILE_POWER_SAVE) {
        /* The A57 cluster is offline: the A53 boost is all there is. */
        ALOGD("interaction: hint %d ms, little %d ms (power save)", duration_hint, little_ms);
        return HINT_HANDLED;
    }
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
    start_profile_watcher();
    std::lock_guard<std::mutex> guard(g_lock);

    if (!data) {
        /* Launch finished: drop the boost early. */
        lock_release(g_launch);
        return HINT_HANDLED;
    }

    if (g_sustained_mask || lock_active(g_launch)) return HINT_HANDLED;

    if (current_profile() == PROFILE_POWER_SAVE) {
        /* A53s only, up to the power save cap. */
        int resources[] = {kLittleMinFreq1248};
        lock_acquire(g_launch, kLaunchBoostMs, resources, count_of(resources));
    } else {
        int resources[] = {kSchedBoostOn, kLittleMinFreq1344, kBigMinFreq1248};
        lock_acquire(g_launch, kLaunchBoostMs, resources, count_of(resources));
    }
    ALOGI("launch boost for %d ms (handle %d)", kLaunchBoostMs, g_launch.handle);
    return HINT_HANDLED;
}

void set_sustained(unsigned bit, bool enabled) {
    std::lock_guard<std::mutex> guard(g_lock);

    unsigned mask = enabled ? (g_sustained_mask | bit) : (g_sustained_mask & ~bit);
    if (mask == g_sustained_mask) return;
    g_sustained_mask = mask;

    if (mask && !CHECK_HANDLE(g_sustained.handle)) {
        release_boost_locks();
        take_sustained_lock();
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
    start_profile_watcher();
    std::lock_guard<std::mutex> guard(g_lock);

    if (!on) {
        g_display_on = false;
        release_boost_locks();
        if (!CHECK_HANDLE(g_display_up.handle)) {
            take_display_off_locks();
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

bool isDeviceSpecificModeSupported(Mode type, bool* _aidl_return) {
    start_profile_watcher();
    /* LAUNCH, INTERACTIVE, SUSTAINED and FIXED_PERFORMANCE are already reported. */
    if (type == Mode::LOW_POWER) {
        /* The framework sends Battery Saver only to a HAL that reports it. */
        *_aidl_return = true;
        return true;
    }
    return false;
}

bool setDeviceSpecificMode(Mode type, bool enabled) {
    start_profile_watcher();
    switch (type) {
        case Mode::LOW_POWER:
            set_low_power(enabled);
            return true;
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
