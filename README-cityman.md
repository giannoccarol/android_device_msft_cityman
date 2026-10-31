# Lumia 950 XL (cityman), LineageOS 18.1 and 20

This tree comes from the Android4Lumia950 talkman (Lumia 950) tree, renamed
for the 950 XL. The phone it was checked against runs the talkman ROM of
2026-05-22 plus the cityman kernel and the changes below, applied by hand.

## LineageOS 20 (Android 13), branch lineage-20-cityman

This branch builds LineageOS 20 for cityman on top of the LineageOS-UL
tree (`LineageOS-UL/android`, `lineage-20.0`) with the local manifest
`lineageos-20/cityman_a13_manifest.xml` of `talkman-manifests`
(branch `lineage-20-cityman`). The first build booted to the launcher on
2026-10-26; the hardware is still being checked.

- Kernel: `android_kernel_mmo_msm8994` `lineage-20-cityman`, which is the
  Android4Lumia950 `lineage-21.0` kernel (eBPF, cgroup2, memcg, cpusets,
  PSI, pidfd and fscrypt backports, userspace kernel version spoof) merged
  with `lineage-18.1-cityman-rom`, plus fixes found while bringing it up
  on cityman: the dentry name snapshot in `vfs_rename2`, the ext4
  `i_extra_isize` growth that deadlocked `/data`, the performance governor
  at boot (the MBA needs it to authenticate the modem image in time) and
  `cpu-boost`.
- HALs: display and media from `hardware/qcom-caf/msm8994`
  (`BOARD_USES_QCOM_HARDWARE`), audio from the generic `hardware/qcom/audio`
  as on 18.1, GPS from `hardware/qcom/gps/msm8994`.
- First-boot choices: no eBPF (`ro.kernel.ebpf.supported=false`), no
  encryption, SELinux permissive.
- The LineageOS projects that build against the LineageOS-UL forks are
  pinned to their 2025-04-05 state (see the manifest); the security patch
  level is 2025-03.
- Performance (from the LineageOS 20 OnePlus 2 port, same SoC): the vendor
  tree ships the MSM8994 `perfd` with its client library, so QTI power HAL
  hints, display-off core limits and thermal-engine profiles reach a real
  daemon (the open-source `libqti-perfd-client` stub is no longer
  installed); core_ctl takes the A57s offline while idle (needs the kernel
  `CONFIG_MSM_CORE_CTL`); the power script adds the HMP scheduler, CPU
  bandwidth, GPU governor and CPU/L2 retention settings; SurfaceFlinger
  color management is off, more layers go to the MDP, SystemUI is compiled
  with `speed`. Not yet verified on the phone.

## Sources

Use `talkman-manifests` branch `lineage-18.1-cityman`,
`lineageos-18.1/cityman_a11_manifest.xml`:

| Path | Repository | Branch |
|---|---|---|
| `device/msft/cityman` | giannoccarol/android_device_msft_cityman | `lineage-18.1-cityman` |
| `kernel/mmo/msm8994` | giannoccarol/android_kernel_mmo_msm8994 | `lineage-18.1-cityman-rom` |
| `vendor/msft/cityman` | giannoccarol/android_vendor_msft_cityman | `lineage-18.1-cityman` |

Build `lineage_cityman-userdebug`. The kernel's `Image.gz-dtb` carries the
talkman and cityman DTBs; LK2ND picks cityman (`androidboot.dtb_idx=1`).

## What differs from the talkman ROM

- Device name `cityman`: `androidboot.hardware=cityman`, `init.cityman.rc`,
  HAL modules `sensors.cityman`, `lights.cityman`, `thermal.cityman`.
  A ROM built from this tree is a new install, not an update of talkman.
- RIL: `libcityman_ds_target`, `libcityman_ril_init` and
  `cityman-uim-bootstrap` from `kernel/mmo/msm8994/tools/cityman-ril`, with
  the preloads in `init.cityman.rc`. See the README there.
- Kernel: `lineage-18.1-cityman-rom` is `lineage-18.1-cityman-next` (memory,
  Wi-Fi, RIL and ADSP fixes) plus a 1.5 A input limit for computer USB-C
  ports. The camera work is on `lineage-18.1-cityman-camera` and is not in
  the ROM kernel until the camera gives pictures.
- Camera: the vendor tree carries the angler 8.1 daemon stack
  (`CAMERA-CITYMAN.md`), but the ROM kernel has no Lumia camera nodes, so no
  camera is listed.
- CPU: all eight cores. `boot_cpus=0-3` starts the A53 cluster, then
  `init.cityman.power.sh` sets the A57 policy and brings cpu4-7 online.
  The kernel holds the A57 cluster without thermal-engine (which does not
  start yet): an A57 core at 80 C caps the cluster at 1.248 GHz, a core
  at 95 C goes offline, the SoC resets at 115 C; BCL still takes cpu6/7
  offline on battery current peaks. The previous `boot_cpus=0-5` came from
  the MSM8992 trees (six cores). Eight cores are untested on the phone.
- Thermal: the kernel (KTM) polls all eight CPU sensors and steps the A57
  cluster down from 75 C until thermal-engine takes over. thermal-engine
  (bullhead blob) starts with the OnePlus 2 `libqti-perfd-client`
  (`perfd` client) and reads `configs/thermal-engine-cityman.conf` (`-c`): A57, A53,
  pop_mem, GPU and board (`msm_therm`) steps, battery charge current,
  shutdown at 68 C on the board. If it stops or crashes, init hands the
  polling back to the kernel (`msm_thermal` `enabled=1`). The thresholds
  are a first conservative set, not yet measured on the phone.

## Firmware

The vendor tree ships the firmware, as the other Lumia vendor trees do:

- Wi-Fi: `qwlan30.bin` (Android SGMT), the cityman `bdwlan30.bin` board file
  and `wlan/qca_cld/WCNSS_qcom_cfg.ini`.
- Bluetooth: `nvm_tlv_3.*.bin`, `rampatch_tlv_3.*.tlv`.
- ADSP for calls: the split of the Cityman `qcadsp8994.mbn`; see
  `kernel/mmo/msm8994/Documentation/arm64/cityman-adsp.txt`.

`bdwlan30.bin` and the Bluetooth files match
`Android4Lumia950/firmware-microsoft-cityman`. Per-device data (MAC
addresses, EFS) stays in `persist` and the modem partitions, which the ROM
does not write.

## Install

From the Android4Lumia950 layout (installer `cityman-temp`, TWRP 3.7.0_9-0):
wipe data in TWRP (the ROM is signed with different keys from the talkman
one), then `adb sideload` the zip. That TWRP reports `angler`, which the
package accepts.

## Not verified

The first ROM from this tree (25/10/2026) boots on one phone; its features
are still being checked. The AP sensors HAL added to the talkman tree after
the installed ROM, and the qcom sensors blobs listed in the vendor tree,
have not been tried on cityman.

## Double tap to wake (LineageOS 23, branch lineage-23-cityman)

The Synaptics S3708 touch firmware (build id 0x2000CA, the image Windows
ships with `WakeupGestureSupported=1`) has the F12 wakeup gesture. The
kernel driver (`synaptics_i2c_rmi4`, branch `dt2w` of the kernel) exposes
`/sys/devices/soc.0/f9923000.i2c/i2c-1/1-004b/wake_gesture`; the QTI power
HAL writes 1/0 there (`qtipower` `tap_to_wake_node`) when "Double tap to
wake" is toggled (`config_supportDoubleTapWake`). With the option on, the
screen-off path keeps the touch controller powered in gesture mode and a
double tap sends `KEY_WAKEUP`. `config_proximityCheckOnWake` (lineage-sdk)
offers the proximity check against wake-ups in a pocket. With always-on
display the panel does not power down, so the touch keeps reporting and
SystemUI handles the double tap itself; the firmware gesture is armed only
when the panel is fully off. Not yet confirmed with a real double tap on
the phone.

## Performance profiles (LineageOS 23, branch lineage-23-cityman)

Settings > Battery > Performance profile, and the "Performance" Quick
Settings tile (add it from the tile editor), choose between three profiles;
the choice survives reboots and Battery Saver uses power save while it is
on:

- **Power save**: the four A57 cores offline, the A53 cores up to 1.25 GHz.
- **Balanced** (default): at most two A57 cores, up to 1.44 GHz.
- **Performance**: all eight cores, A57 up to 1.96 GHz; thermal-engine still
  throttles at 72 °C.

The power HAL holds each profile as a perfd lock, so core_ctl, perfd and
thermal-engine keep working together. The page and the tile come from the
device app `CitymanParts`
(`giannoccarol/android_packages_apps_CitymanParts`), since LineageOS 23 has no performance
profiles of its own. Details and opcodes: `power/README.md`.

## Rear flash (LineageOS 23, branch lineage-23-cityman)

The rear LEDs hang off the "Hill" flash IC on CCI master 0 (8-bit 0x60, ID
0x16), as on the Lumia 950, not off PMI8994. With a kernel that has the
Hill nodes, `libflash_hill` (`camera-sensors/hill-flash`) gives the camera
preflash, flash and video torch, and the flashlight tile uses the kernel's
device tree sequence. `CITYMAN_CAMERA_FLASH=false` leaves the flash out.
Not yet verified on the 950 XL; details in `camera-sensors/hill-flash/README.md`.

## Credits

This ROM builds on the work of others:

- **Android4Lumia950** (djokcho, EpicLPer, j0sh1x and the community): the
  talkman device, vendor and kernel trees, the installer, TWRP and the
  `firmware-microsoft-cityman` files this ROM ships.
- **archienz** (`archienz/android_device_msft_talkman`,
  `archienz/android_kernel_mmo_msm8994`): rear camera measurements on the
  Lumia 950 (CSI lane map `0x0423`, 24 MHz MCLK, CCI layout), used on the
  cityman camera branch.
- **EpicLPer** (Android4Lumia950 kernel, talkman): the TAS2552/TAS2553
  speaker amplifier driver and the Quaternary MI2S wiring the cityman audio
  uses (mmo kernel `e4d0c025066`, `9bd1bb032d4`, `22039ccd1ea`,
  `5b52570b88d`).
- **shadowofleaf96** (`shadowofleaf96/android_device_msft_cityman`,
  `shadowofleaf96/android_kernel_msft_cityman`): the cityman audio
  bring-up this tree's audio is ported from. Mixer paths, speaker and call
  routing, BT SCO and DMIC paths, the 16-bit input profiles, disabled
  speaker protection and the Bluetooth Audio HAL come from his device tree
  (`1900285`, `d76d600`, `ffc26fd`, `2e678be`, `f70ecd0`, `83955b9`,
  `02ece9d`, `9a5933d`, `6616a8a`, `e3a93a0`, `431558e`); the Cityman Q6
  MI2S node, the removed WCD9330 speaker supplies and the ASM topology
  fallback in the mmo kernel follow his kernel (`efb1ebf`, `1a51144`), as
  do the MSM8994 eMMC clock rates (`4bc7ab2`), the sensor and NFC includes
  for cityman (`cd01dc9`, `386e7a0`; the sensor buses themselves are
  EpicLPer's `7d96420fc1e`) and the nq-nci resume fix (`628c06f`).
- **Google/LGE** (bullhead) and **Huawei/Google** (angler): the power
  script and its interactive tuning come from bullhead; the eight-core
  bring-up order (`boot_cpus=0-3`, A57 policy before cpu5-7) follows
  angler's stock OPM7 script as kept in
  `derveror/android_device_huawei_angler` (`7fc8940`), and
  `thermal-engine-angler.conf` is the reference for the cityman CPU
  thermal limits.
- **EpicLPer** (Android4Lumia950 talkman, `f1e32adfeb3` on
  `lineage-18.1-talkman-power`): the `libqti-perfd-client` stub that lets
  thermal-engine start, also used by shadowofleaf96 (`67c6195`).
- **LineageOS OnePlus 2** (`LineageOS/android_device_oneplus_oneplus2`
  `8e47d59`, `LineageOS/android_kernel_oneplus_msm8994` lineage-17.1):
  the same MSM8994 bring-up and the core_ctl settings evaluated for
  cityman.
- **mukul2259** and **_undead** (LineageOS 20 for OnePlus 2,
  `mukul2259/experimental_device_oneplus_oneplus2` `6cdfa90`,
  `LineageOS-OP2`): the core_ctl, HMP scheduler, CPU bandwidth, GPU,
  retention, HWC, ART and log properties used on Android 13, and the
  `perfd` setup. **davigamer987** (`davigamer987/vendor_oneplus_oneplus2`
  `1abfce3`): the OnePlus 2 `perfd` blobs.
- **LineageOS** and **TheMuppets**: the Android 11 base and the reference
  vendor blobs.
- **LineageOS-UL** (`LineageOS-UL/android` and its forks): the Android 13
  base with the no-eBPF, cgroup v1 freezer and legacy fixes that let it
  run on a 3.10 kernel, and the msm8994 display HAL.
- **ximi-libra-test** (`ximi-libra-test/android_device_xiaomi_libra`
  `lineage-20-rev`, backslashxx, rik0612c and contributors): the MSM8992
  LineageOS 20 tree whose layout this branch follows (qcom-caf HALs,
  Bluetooth profile properties, APCF off, `vendor.rild.libpath`, ELF
  prebuilts) and the wpa_supplicant format patch kept in
  `talkman-manifests`.
- **Android4Lumia950 `lineage-21.0` kernel** (j0sh1x, with backports by
  Yaroslav Zviezda/acroreiser, Derek and mukul2259): the eBPF, cgroup,
  PSI and fscrypt backports and the kernel version spoof this branch's
  kernel is based on.
- **WOA-Project/SynapticsTouch** (Bingxing Wang and the LumiaWoA authors):
  the F12 CTRL20 gesture sequence and the DATA4 double tap code on the
  Lumia 950 XL used by the kernel double tap to wake; the timing (350 ms,
  3 mm) comes from Microsoft's Windows driver package for the 950 XL.
  **Synaptics DSX 2.5** of the Nexus 6P kernel
  (`Android4Lumia950/angler-1`) and the OnePlus 2 S3320 driver
  (`LineageOS/android_kernel_oneplus_msm8994`, `lineage-18.0`) for the
  register offsets and the suspend/resume handling;
  `LineageOS/android_device_oneplus_oneplus2` as the reference for the
  Android side.

- **hraj9258** (Himanshu Raj, `hraj9258/android_device_msft_talkman`
  `lineage-18.1-talkman`, `hraj9258/android_kernel_mmo_msm8994`
  `talkman-cam-clean`): the Hill flash of the Lumia 950, its register map
  and operating points, `flash_lib_hill.c` (built here as `libflash_hill`)
  and the kernel flash driver changes (`bb9466c`); the rear chromatix, AF
  and actuator tuning in `camera-sensors/rear-tuning`.

Work under evaluation for cityman, credited where it is used:
**hraj9258** (talkman rear camera with a kernel SMIA++ IMX230 driver).
