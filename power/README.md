# Power HAL extension (cityman)

`libpower_ext_cityman` is linked into the QTI power HAL
(`vendor/qcom/opensource/power`, `android.hardware.power-service-qti`)
through the soong config variable `qtipower`/`mode_ext_lib`:

```make
# BoardConfig.mk
$(call soong_config_set,qtipower,mode_ext_lib,libpower_ext_cityman)
```

The QTI HAL sends interaction and launch boosts with `perf_hint()`. The
MSM8994 perfd client library shipped here (`libqti-perfd-client.so`, OnePlus 2)
exports only `perf_lock_acq`/`perf_lock_rel`, so without this extension every
boost is a no-op. The extension overrides `power_hint_override()`,
`set_interactive_override()` and `setDeviceSpecificMode()` and takes perf locks
with the 16-bit MSM8994 perfd opcodes `0xRRVV` (resource `RR`, value `VV`;
CPU frequencies in units of 100 MHz, rounded by perfd to the cluster table).

| Event | Opcodes | Meaning on cityman | Duration |
|---|---|---|---|
| Interaction (touch) | `0x20C` | A53 min ≥ 1.2 GHz (1248 MHz) | 1.5 s (hint + 0.75 s for long hints) |
| | `0x1F08` | A57 min ≥ 0.8 GHz (864 MHz) | 0.5 s |
| | `0x4F00 \| n/2` | `sched_downmigrate` = upmigrate / 2 | 1 s (hint, max 5 s) |
| | `0x4E00 \| n` | `sched_upmigrate` 20 (sporadic touch, fling) .. 87 (rapid touches) | 0.5 s |
| Launch | `0x1E01`, `0x20D`, `0x1F0C` | `sched_boost` on, A53 min ≥ 1344 MHz, A57 min ≥ 1248 MHz | 1.5 s, released early when the launch ends |
| Sustained / fixed performance | `0x1509`, `0x230C` | A53 max ~0.9 GHz, A57 max ~1.2 GHz; touch and launch boosts off | while the mode is on |
| Screen off | `0x4E63`, `0x4F5F` | `sched_upmigrate` 99, `sched_downmigrate` 95 | until the screen is on |

Differences from the sources below: the angler interaction boost also took
`0x2312` (A57 max 1.8 GHz). On cityman `init.cityman.power.sh` caps the A57s
at 1632 MHz and msm_performance can only lower a limit, so it would do
nothing and is left out. Angler's sustained mode hot-plugged the A57s out
with `0x3DFF` and capped the GPU through sysfs; the `0x3D` resource is not
verified on the OnePlus 2 perfd, so here the A57s are capped with the `0x23`
resource instead and the GPU is left alone. The kernel rejects a
`sched_downmigrate` above `sched_upmigrate`, so the two are separate locks,
taken and released in an order that keeps them valid.

## Performance profiles

The extension also holds a performance profile, chosen in Settings >
Battery > Performance profile or with the "Performance" Quick Settings tile
(app `CitymanParts`, system uid, no launcher entry, repository
`giannoccarol/android_packages_apps_CitymanParts` at `packages/apps/CitymanParts`). LineageOS 23
no longer has the LineageOS performance profiles (`PerformanceManager` was
removed from lineage-sdk), so the page is injected into Settings by the
device app.

| Profile | perf lock (held until the profile changes) | Result on cityman |
|---|---|---|
| Power save (0) | `0x3DFF`, `0x150C` | A57 cluster offline (`online` = `0-3`), A53 max 1248 MHz |
| Balanced (1, default) | `0x3DFD`, `0x230E` | at most two A57 online, A57 max 1440 MHz |
| Performance (2) | none | core_ctl decides up to four A57, A57 up to 1958 MHz |

- The app stores the choice in `persist.vendor.power.profile`; the HAL follows
  it (a thread waits for property changes) and publishes the profile in use
  in `vendor.power.profile.active`. Battery Saver (`Mode::LOW_POWER`, now
  reported as supported) applies power save while it is on and leaves the
  stored choice alone.
- `0x3D` is the online core limit of the A57 cluster (value `0xFF - n`).
  The OnePlus 2 perfd applies it through core_ctl (`cpu4/core_ctl/max_cpus`;
  the kernel clamps `min_cpus` to it) and on release writes back
  `ro.core_ctl_max_cpu` (4) while `min_cpus` returns to 2. core_ctl refuses
  to online a CPU above its limit, so this is the hotplug path that does not
  fight core_ctl; `msm_performance/max_cpus` would. Frequency limits go
  through msm_performance like every other perf lock, and thermal-engine and
  msm_thermal still lower them further.
- perfd rounds a frequency up to the next step of the cluster table
  (`0x230E` 1400 → 1440 MHz, `0x150C` 1200 → 1248 MHz). Power save caps the
  A53s at 1248 MHz and balanced the A57s at 1440 MHz: the DDR floor follows
  the CPU frequency (`devfreq_cpufreq` in the kernel DT), so these caps also
  keep the bus at 3509 / 4066 instead of 5928. 1440 MHz is also where
  thermal-engine puts the A57s at 76 °C.
- Performance lifts the A57 user maximum from the boot value (1632 MHz,
  `init.cityman.power.sh`) to `cpuinfo_max_freq`: a perf lock can only
  lower a limit, so the HAL sets `vendor.power.big_max_freq` (`full` or
  `default`) and init runs `init.cityman.big-max-freq.sh` as root. The boot
  keeps the 1632 MHz cap until perfd runs.
- In power save the interaction boost keeps only the A53 lock and the launch
  boost is `0x20C` (A53 at the 1248 MHz cap): `sched_boost` and the A57
  locks would point at offline CPUs.
- perfd starts at `sys.boot_completed`; init then sets `vendor.power.perfd`
  (and again from `onrestart`), and the HAL takes the profile lock (retrying
  for 10 s while perfd opens its socket). After a perfd restart the HAL
  forgets its old handles instead of releasing numbers that may belong to
  other clients, and takes the screen-off and sustained locks back.

Checks: `cat /sys/devices/system/cpu/online`,
`cat /sys/devices/system/cpu/cpu4/core_ctl/max_cpus`,
`cat /sys/module/msm_performance/parameters/cpu_max_freq`,
`getprop vendor.power.profile.active`, `logcat -s "QTI PowerHAL cityman"`.
To choose a profile from a shell: `setprop persist.vendor.power.profile 0`
(1, 2).

## Credits

- LineageOS, `android_device_huawei_angler` (Nexus 6P, MSM8994), branch
  `lineage-17.1`, commit `9d6ffc1de649`: `power/power.c` (interaction boost
  `0x20C`, `0x1F08`, `0x4E00`/`0x4F00` with the upmigrate curve and durations;
  sustained performance `0x1509`) and `power/power-8994.c` (meaning of
  `0x150C`, `0x1F03`, `0x2303`). Original code by Google and The Linux
  Foundation.
- LineageOS, `android_hardware_qcom_power`, branch `lineage-16.0`, commit
  `8b038b1f648f`: `power-8994.c` (launch boost with `SCHED_BOOST_ON` and
  `0x20C`, screen-off `0x4E63`/`0x4F5F`).
- OnePlus 2 perfd blobs (vendor tree): `perf-profile4.conf` documents `0x2310`
  as the A57 `scaling_max_freq` at 1.6 GHz, which confirms the opcode format of
  this perfd.
- The Linux Foundation / Qualcomm Innovation Center, QTI power HAL
  (`vendor/qcom/opensource/power`): the extension hooks.
- The angler sustained mode above is also the source of the `0x3DFF`
  opcode (A57 cluster offline) used by power save. The profile numbers
  (power save 0, balanced 1, performance 2) are those of the LineageOS
  power HAL profiles (`LineageOS/android_hardware_qcom_power`).
- Google, Material icon "speed" (Apache 2.0): the icon of the Settings
  entry and of the tile.
