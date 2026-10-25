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
- `boot_cpus=0-5` is kept from talkman: six of eight cores online, the
  setting verified on this phone. All eight cores are untested.

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
- **LineageOS** and **TheMuppets**: the Android 11 base and the reference
  vendor blobs.

Work under evaluation for cityman, credited where it is used:
**shadowofleaf96** (cityman trees, TAS2553 / QUAT_MI2S audio and call
routing) and **hraj9258** (talkman rear camera with a kernel SMIA++ IMX230
driver).
