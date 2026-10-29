# AIDL thermal HAL for cityman

`android.hardware.thermal-service.cityman` serves
`android.hardware.thermal.IThermal/default`, interface version 3 (the version
the Android 16 framework compatibility matrices 202504/202604 ask for). It
lets `ThermalManagerService` see real temperatures and a throttling status.
It does not throttle anything: thermal-engine
(`configs/thermal-engine-cityman.conf`) and the kernel KTM keep doing that.
The legacy libhardware module in `thermal/` (`thermal.cityman`) is left as it
is; Android 16 does not load it.

## Sensors

Zones are looked up by their `type` at start, so the `thermal_zoneN` numbering
does not matter. A sensor whose zone is missing is left out of the list.

| Name        | Zone type                | Type    | Unit on 3.10 |
|-------------|--------------------------|---------|--------------|
| cpu0-cpu3   | tsens_tz_sensor7/8/9/10  | CPU     | °C           |
| cpu4-cpu7   | tsens_tz_sensor13/14/15/6| CPU     | °C           |
| gpu         | tsens_tz_sensor12        | GPU     | °C           |
| battery     | battery                  | BATTERY | m°C          |
| skin        | msm_therm (board)        | SKIN    | °C           |

The CPU/GPU mapping is the `qcom,sensor-information` aliases of
`msm8994.dtsi`. A °C zone that reads 1000 or more is taken as millidegrees;
readings outside -50..150 °C are ignored (the last good value is kept).

## Levels (°C)

Index order: LIGHT, MODERATE, SEVERE, CRITICAL, EMERGENCY, SHUTDOWN
(`-` = not used).

| Sensor   | LIGHT | MODERATE | SEVERE | CRITICAL | EMERGENCY | SHUTDOWN | Hyst. |
|----------|-------|----------|--------|----------|-----------|----------|-------|
| A53 cpu  | 85    | 90       | -      | -        | 95        | 110      | 3     |
| A57 cpu  | 72    | 76       | 80     | 85       | 95        | 110      | 3     |
| gpu      | 80    | 85       | 90     | -        | -         | 110      | 3     |
| battery  | 42    | 44       | 45     | -        | -         | 60       | 2     |
| skin     | 50    | 54       | 58     | 60       | -         | 68       | 2     |

LIGHT to CRITICAL are the thermal-engine steps for that sensor; 95 °C is
where the KTM takes cores offline; tsens resets the SoC at 115 °C; skin
SHUTDOWN is the thermal-engine shutdown rule on msm_therm. The framework
powers the phone off when a CPU, GPU, SKIN or BATTERY sensor reports
SHUTDOWN, so that level is only reported after it has lasted 1.5 s.
The framework's overall thermal status and headroom come from `skin`
(SEVERE = 58 °C).

A thread reads the sensors every 2 s while a callback is registered and
calls `notifyThrottling` when a level changes. No cooling devices are
reported and `forecastSkinTemperature` returns `EX_UNSUPPORTED_OPERATION`.
`dumpsys android.hardware.thermal.IThermal/default` prints the current
values.

## Credits

- Service layout, callback handling (`interfacesEqual`) and the shape of the
  method set follow the AOSP example implementation,
  `hardware/interfaces/thermal/aidl/default` (The Android Open Source
  Project, Apache-2.0).
- Sensor mapping and levels come from this tree's
  `configs/thermal-engine-cityman.conf` and from the msm8994 kernel device
  tree of `android_kernel_mmo_msm8994` (Android4Lumia950 and its community).
