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

#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <aidl/android/hardware/thermal/BnThermal.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace impl {
namespace cityman {

// One entry per ThrottlingSeverity value, NONE (0) to SHUTDOWN (6).
constexpr size_t kNumSeverities = static_cast<size_t>(ThrottlingSeverity::SHUTDOWN) + 1;

struct SensorConfig {
    std::string name;      // name reported to the framework
    std::string zoneType;  // /sys/class/thermal/thermal_zone*/type
    TemperatureType type;
    bool milliCelsius;     // true if the zone reports millidegrees
    std::array<float, kNumSeverities> hot;  // Celsius, NAN = level unused
    float hysteresis;      // Celsius, for going back down a level
};

class Thermal : public BnThermal {
  public:
    Thermal();
    ~Thermal() override;

    ndk::ScopedAStatus getCoolingDevices(std::vector<CoolingDevice>* out_devices) override;
    ndk::ScopedAStatus getCoolingDevicesWithType(CoolingType in_type,
                                                 std::vector<CoolingDevice>* out_devices) override;

    ndk::ScopedAStatus getTemperatures(std::vector<Temperature>* out_temperatures) override;
    ndk::ScopedAStatus getTemperaturesWithType(TemperatureType in_type,
                                               std::vector<Temperature>* out_temperatures) override;

    ndk::ScopedAStatus getTemperatureThresholds(
            std::vector<TemperatureThreshold>* out_temperatureThresholds) override;
    ndk::ScopedAStatus getTemperatureThresholdsWithType(
            TemperatureType in_type,
            std::vector<TemperatureThreshold>* out_temperatureThresholds) override;

    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& in_callback) override;
    ndk::ScopedAStatus registerThermalChangedCallbackWithType(
            const std::shared_ptr<IThermalChangedCallback>& in_callback,
            TemperatureType in_type) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& in_callback) override;

    ndk::ScopedAStatus registerCoolingDeviceChangedCallbackWithType(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback,
            CoolingType in_type) override;
    ndk::ScopedAStatus unregisterCoolingDeviceChangedCallback(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& in_callback) override;

    ndk::ScopedAStatus forecastSkinTemperature(int32_t in_forecastSeconds,
                                               float* _aidl_return) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    struct Sensor {
        SensorConfig config;
        std::string path;  // .../thermal_zoneN/temp
        float value = NAN;  // last valid reading, Celsius
        ThrottlingSeverity severity = ThrottlingSeverity::NONE;
        ThrottlingSeverity notified = ThrottlingSeverity::NONE;
        bool overShutdown = false;
        std::chrono::steady_clock::time_point overShutdownSince;
    };

    struct ThermalCallback {
        std::shared_ptr<IThermalChangedCallback> callback;
        bool filterType;
        TemperatureType type;
    };

    // Reads one sensor and updates its value and severity. Needs sensors_mutex_.
    Temperature sampleLocked(Sensor& sensor);
    static ThrottlingSeverity severityFor(const SensorConfig& config, float celsius,
                                          ThrottlingSeverity previous);
    static TemperatureThreshold thresholdFor(const SensorConfig& config);
    ndk::ScopedAStatus addThermalCallback(const std::shared_ptr<IThermalChangedCallback>& cb,
                                          bool filterType, TemperatureType type);
    void pollLoop();

    std::mutex sensors_mutex_;
    std::vector<Sensor> sensors_;

    std::mutex callback_mutex_;
    std::vector<ThermalCallback> thermal_callbacks_;
    std::vector<std::shared_ptr<ICoolingDeviceChangedCallback>> cdev_callbacks_;

    std::mutex poll_mutex_;
    std::condition_variable poll_cv_;
    bool stop_ = false;
    std::thread poll_thread_;
};

}  // namespace cityman
}  // namespace impl
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
