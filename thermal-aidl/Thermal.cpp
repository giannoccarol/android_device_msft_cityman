/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define LOG_TAG "android.hardware.thermal-service.cityman"

#include "Thermal.h"

#include <dirent.h>

#include <algorithm>
#include <cstdlib>
#include <map>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace impl {
namespace cityman {

using ndk::ScopedAStatus;

namespace {

constexpr char kThermalSysfsDir[] = "/sys/class/thermal";
constexpr auto kPollInterval = std::chrono::seconds(2);
// A SHUTDOWN level must last this long before it is reported, so that a
// single bad tsens sample cannot make the framework power the phone off.
constexpr auto kShutdownConfirm = std::chrono::milliseconds(1500);
// Readings outside this range (Celsius) are treated as read errors.
constexpr float kMinValid = -50.0f;
constexpr float kMaxValid = 150.0f;

// Levels follow configs/thermal-engine-cityman.conf and the kernel KTM limits
// (cityman-thermal.dtsi): thermal-engine and the kernel do the throttling,
// these values only tell the framework how far along they are.
// Index: NONE, LIGHT, MODERATE, SEVERE, CRITICAL, EMERGENCY, SHUTDOWN.
//
// tsens zones and msm_therm report degrees C on this 3.10 kernel; battery
// (power_supply) reports millidegrees.
const std::vector<SensorConfig> kSensorConfigs = {
        // A53 cluster: thermal-engine steps at 85/90 C, KTM offlines a core at 95 C,
        // tsens resets the SoC at 115 C.
        {"cpu0", "tsens_tz_sensor7", TemperatureType::CPU, false,
         {NAN, 85.0f, 90.0f, NAN, NAN, 95.0f, 110.0f}, 3.0f},
        {"cpu1", "tsens_tz_sensor8", TemperatureType::CPU, false,
         {NAN, 85.0f, 90.0f, NAN, NAN, 95.0f, 110.0f}, 3.0f},
        {"cpu2", "tsens_tz_sensor9", TemperatureType::CPU, false,
         {NAN, 85.0f, 90.0f, NAN, NAN, 95.0f, 110.0f}, 3.0f},
        {"cpu3", "tsens_tz_sensor10", TemperatureType::CPU, false,
         {NAN, 85.0f, 90.0f, NAN, NAN, 95.0f, 110.0f}, 3.0f},
        // A57 cluster: thermal-engine steps at 72/76/80/85 C.
        {"cpu4", "tsens_tz_sensor13", TemperatureType::CPU, false,
         {NAN, 72.0f, 76.0f, 80.0f, 85.0f, 95.0f, 110.0f}, 3.0f},
        {"cpu5", "tsens_tz_sensor14", TemperatureType::CPU, false,
         {NAN, 72.0f, 76.0f, 80.0f, 85.0f, 95.0f, 110.0f}, 3.0f},
        {"cpu6", "tsens_tz_sensor15", TemperatureType::CPU, false,
         {NAN, 72.0f, 76.0f, 80.0f, 85.0f, 95.0f, 110.0f}, 3.0f},
        {"cpu7", "tsens_tz_sensor6", TemperatureType::CPU, false,
         {NAN, 72.0f, 76.0f, 80.0f, 85.0f, 95.0f, 110.0f}, 3.0f},
        // GPU: thermal-engine steps at 80/85/90 C.
        {"gpu", "tsens_tz_sensor12", TemperatureType::GPU, false,
         {NAN, 80.0f, 85.0f, 90.0f, NAN, NAN, 110.0f}, 3.0f},
        // Battery: charge current mitigation at 42/44/45 C.
        {"battery", "battery", TemperatureType::BATTERY, true,
         {NAN, 42.0f, 44.0f, 45.0f, NAN, NAN, 60.0f}, 2.0f},
        // Board sensor near the SoC, used as skin: thermal-engine steps at
        // 50-60 C and shuts the phone down at 68 C.
        {"skin", "msm_therm", TemperatureType::SKIN, false,
         {NAN, 50.0f, 54.0f, 58.0f, 60.0f, NAN, 68.0f}, 2.0f},
};

bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface>& left,
                     const std::shared_ptr<::ndk::ICInterface>& right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

// Maps thermal zone type -> .../thermal_zoneN directory.
std::map<std::string, std::string> scanThermalZones() {
    std::map<std::string, std::string> zones;
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(kThermalSysfsDir), closedir);
    if (!dir) {
        PLOG(ERROR) << "Cannot open " << kThermalSysfsDir;
        return zones;
    }
    while (struct dirent* entry = readdir(dir.get())) {
        const std::string entryName = entry->d_name;
        if (!::android::base::StartsWith(entryName, "thermal_zone")) continue;
        const std::string zoneDir = std::string(kThermalSysfsDir) + "/" + entryName;
        std::string type;
        if (!::android::base::ReadFileToString(zoneDir + "/type", &type)) continue;
        type = ::android::base::Trim(type);
        if (!type.empty()) zones.emplace(type, zoneDir);
    }
    return zones;
}

bool readZoneCelsius(const std::string& path, bool milliCelsius, float* out) {
    std::string buf;
    if (path.empty() || !::android::base::ReadFileToString(path, &buf)) return false;
    long long raw = 0;
    if (!::android::base::ParseInt(::android::base::Trim(buf), &raw)) return false;
    float celsius;
    if (milliCelsius || std::llabs(raw) >= 1000) {
        // Also catches a zone that moved to millidegrees with a newer kernel.
        celsius = static_cast<float>(raw) / 1000.0f;
    } else {
        celsius = static_cast<float>(raw);
    }
    if (std::isnan(celsius) || celsius < kMinValid || celsius > kMaxValid) return false;
    *out = celsius;
    return true;
}

}  // namespace

Thermal::Thermal() {
    const auto zones = scanThermalZones();
    for (const auto& config : kSensorConfigs) {
        auto it = zones.find(config.zoneType);
        if (it == zones.end()) {
            LOG(WARNING) << "No thermal zone of type " << config.zoneType << " for "
                         << config.name << ", not reported";
            continue;
        }
        Sensor sensor;
        sensor.config = config;
        sensor.path = it->second + "/temp";
        LOG(INFO) << config.name << " -> " << sensor.path;
        sensors_.push_back(std::move(sensor));
    }
    {
        std::lock_guard<std::mutex> lock(sensors_mutex_);
        for (auto& sensor : sensors_) sampleLocked(sensor);
        for (auto& sensor : sensors_) sensor.notified = sensor.severity;
    }
    poll_thread_ = std::thread(&Thermal::pollLoop, this);
}

Thermal::~Thermal() {
    {
        std::lock_guard<std::mutex> lock(poll_mutex_);
        stop_ = true;
    }
    poll_cv_.notify_all();
    if (poll_thread_.joinable()) poll_thread_.join();
}

ThrottlingSeverity Thermal::severityFor(const SensorConfig& config, float celsius,
                                        ThrottlingSeverity previous) {
    size_t level = 0;
    for (size_t i = 1; i < kNumSeverities; i++) {
        if (!std::isnan(config.hot[i]) && celsius >= config.hot[i]) level = i;
    }
    // Hysteresis: stay at a higher level until the temperature drops below
    // that level's threshold minus the hysteresis.
    size_t held = static_cast<size_t>(previous);
    if (held >= kNumSeverities) held = 0;
    while (held > level) {
        if (!std::isnan(config.hot[held]) && celsius >= config.hot[held] - config.hysteresis) {
            break;
        }
        held--;
    }
    return static_cast<ThrottlingSeverity>(std::max(level, held));
}

TemperatureThreshold Thermal::thresholdFor(const SensorConfig& config) {
    TemperatureThreshold threshold;
    threshold.type = config.type;
    threshold.name = config.name;
    threshold.hotThrottlingThresholds.assign(config.hot.begin(), config.hot.end());
    threshold.coldThrottlingThresholds.assign(kNumSeverities, NAN);
    return threshold;
}

Temperature Thermal::sampleLocked(Sensor& sensor) {
    float celsius;
    if (readZoneCelsius(sensor.path, sensor.config.milliCelsius, &celsius)) {
        sensor.value = celsius;
        ThrottlingSeverity severity = severityFor(sensor.config, celsius, sensor.severity);
        if (severity == ThrottlingSeverity::SHUTDOWN) {
            const auto now = std::chrono::steady_clock::now();
            if (!sensor.overShutdown) {
                sensor.overShutdown = true;
                sensor.overShutdownSince = now;
            }
            if (now - sensor.overShutdownSince < kShutdownConfirm) {
                severity = ThrottlingSeverity::EMERGENCY;
            }
        } else {
            sensor.overShutdown = false;
        }
        sensor.severity = severity;
    } else {
        // Keep the last value and level; never raise the level on stale data.
        sensor.overShutdown = false;
        LOG(VERBOSE) << "Cannot read " << sensor.path;
    }

    Temperature temperature;
    temperature.type = sensor.config.type;
    temperature.name = sensor.config.name;
    temperature.value = sensor.value;
    temperature.throttlingStatus = sensor.severity;
    return temperature;
}

ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out_devices) {
    // Mitigation belongs to thermal-engine and the kernel; nothing is exposed.
    out_devices->clear();
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType /* in_type */,
                                                 std::vector<CoolingDevice>* out_devices) {
    out_devices->clear();
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* out_temperatures) {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    out_temperatures->clear();
    for (auto& sensor : sensors_) out_temperatures->push_back(sampleLocked(sensor));
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType in_type,
                                               std::vector<Temperature>* out_temperatures) {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    out_temperatures->clear();
    for (auto& sensor : sensors_) {
        if (sensor.config.type == in_type) out_temperatures->push_back(sampleLocked(sensor));
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* out_temperatureThresholds) {
    out_temperatureThresholds->clear();
    for (const auto& sensor : sensors_) {
        out_temperatureThresholds->push_back(thresholdFor(sensor.config));
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType in_type, std::vector<TemperatureThreshold>* out_temperatureThresholds) {
    out_temperatureThresholds->clear();
    for (const auto& sensor : sensors_) {
        if (sensor.config.type == in_type) {
            out_temperatureThresholds->push_back(thresholdFor(sensor.config));
        }
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::addThermalCallback(const std::shared_ptr<IThermalChangedCallback>& cb,
                                          bool filterType, TemperatureType type) {
    if (cb == nullptr) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Invalid nullptr callback");
    }
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (std::any_of(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                        [&](const ThermalCallback& c) {
                            return interfacesEqual(c.callback, cb);
                        })) {
            return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                               "Callback already registered");
        }
        thermal_callbacks_.push_back({cb, filterType, type});
    }
    poll_cv_.notify_all();
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    return addThermalCallback(in_callback, false, TemperatureType::UNKNOWN);
}

ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& in_callback, TemperatureType in_type) {
    return addThermalCallback(in_callback, true, in_type);
}

ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& in_callback) {
    if (in_callback == nullptr) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Invalid nullptr callback");
    }
    std::lock_guard<std::mutex> lock(callback_mutex_);
    auto it = std::find_if(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                           [&](const ThermalCallback& c) {
                               return interfacesEqual(c.callback, in_callback);
                           });
    if (it == thermal_callbacks_.end()) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Callback wasn't registered");
    }
    thermal_callbacks_.erase(it);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::registerCoolingDeviceChangedCallbackWithType(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback,
        CoolingType /* in_type */) {
    // Accepted for API compatibility; no cooling devices are reported, so it
    // is never called.
    if (in_callback == nullptr) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Invalid nullptr callback");
    }
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (std::any_of(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                    [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                        return interfacesEqual(c, in_callback);
                    })) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Callback already registered");
    }
    cdev_callbacks_.push_back(in_callback);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterCoolingDeviceChangedCallback(
        const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) {
    if (in_callback == nullptr) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Invalid nullptr callback");
    }
    std::lock_guard<std::mutex> lock(callback_mutex_);
    auto it = std::find_if(cdev_callbacks_.begin(), cdev_callbacks_.end(),
                           [&](const std::shared_ptr<ICoolingDeviceChangedCallback>& c) {
                               return interfacesEqual(c, in_callback);
                           });
    if (it == cdev_callbacks_.end()) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Callback wasn't registered");
    }
    cdev_callbacks_.erase(it);
    return ScopedAStatus::ok();
}

ScopedAStatus Thermal::forecastSkinTemperature(int32_t /* in_forecastSeconds */,
                                               float* /* _aidl_return */) {
    return ScopedAStatus::fromExceptionCodeWithMessage(EX_UNSUPPORTED_OPERATION,
                                                       "Skin forecast not supported");
}

void Thermal::pollLoop() {
    std::unique_lock<std::mutex> pollLock(poll_mutex_);
    while (!stop_) {
        poll_cv_.wait_for(pollLock, kPollInterval, [this] { return stop_; });
        if (stop_) break;
        pollLock.unlock();

        std::vector<ThermalCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            callbacks = thermal_callbacks_;
        }

        if (!callbacks.empty()) {
            std::vector<Temperature> changed;
            {
                std::lock_guard<std::mutex> lock(sensors_mutex_);
                for (auto& sensor : sensors_) {
                    Temperature temperature = sampleLocked(sensor);
                    if (sensor.severity != sensor.notified) {
                        LOG(INFO) << sensor.config.name << " " << temperature.value << " C: "
                                  << toString(sensor.notified) << " -> "
                                  << toString(sensor.severity);
                        sensor.notified = sensor.severity;
                        changed.push_back(temperature);
                    }
                }
            }

            std::vector<std::shared_ptr<IThermalChangedCallback>> dead;
            for (const auto& temperature : changed) {
                for (const auto& cb : callbacks) {
                    if (cb.filterType && cb.type != temperature.type) continue;
                    ScopedAStatus ret = cb.callback->notifyThrottling(temperature);
                    if (!ret.isOk()) {
                        LOG(ERROR) << "notifyThrottling failed: " << ret.getDescription();
                        if (ret.getStatus() == STATUS_DEAD_OBJECT) dead.push_back(cb.callback);
                    }
                }
            }
            if (!dead.empty()) {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                thermal_callbacks_.erase(
                        std::remove_if(thermal_callbacks_.begin(), thermal_callbacks_.end(),
                                       [&](const ThermalCallback& c) {
                                           return std::any_of(
                                                   dead.begin(), dead.end(),
                                                   [&](const std::shared_ptr<
                                                           IThermalChangedCallback>& d) {
                                                       return interfacesEqual(c.callback, d);
                                                   });
                                       }),
                        thermal_callbacks_.end());
            }
        }

        pollLock.lock();
    }
}

binder_status_t Thermal::dump(int fd, const char** /* args */, uint32_t /* numArgs */) {
    std::string out = "cityman thermal HAL (reporting only; thermal-engine/KTM throttle)\n";
    {
        std::lock_guard<std::mutex> lock(sensors_mutex_);
        for (const auto& sensor : sensors_) {
            out += ::android::base::StringPrintf(
                    "  %-8s %-6s %-48s %7.1f C  %s\n", sensor.config.name.c_str(),
                    toString(sensor.config.type).c_str(), sensor.path.c_str(), sensor.value,
                    toString(sensor.severity).c_str());
        }
    }
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        out += ::android::base::StringPrintf("  thermal callbacks: %zu\n",
                                             thermal_callbacks_.size());
    }
    ::android::base::WriteStringToFd(out, fd);
    return STATUS_OK;
}

}  // namespace cityman
}  // namespace impl
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
