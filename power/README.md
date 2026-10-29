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
