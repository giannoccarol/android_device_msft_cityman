# Lumia 950 XL (cityman), LineageOS 18.1

This tree comes from the Android4Lumia950 talkman (Lumia 950) tree, renamed
for the 950 XL. The phone it was checked against runs the talkman ROM of
2026-05-22 plus the cityman kernel and the changes below, applied by hand.

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
  (bullhead blob) starts with the open-source `libqti-perfd-client` stub
  and reads `configs/thermal-engine-cityman.conf` (`-c`): A57, A53,
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
  fallback in the mmo kernel follow his kernel (`efb1ebf`, `1a51144`).
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
- **LineageOS** and **TheMuppets**: the Android 11 base and the reference
  vendor blobs.

Work under evaluation for cityman, credited where it is used:
**hraj9258** (talkman rear camera with a kernel SMIA++ IMX230 driver).
