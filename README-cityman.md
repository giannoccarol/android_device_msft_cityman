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
| `kernel/mmo/msm8994` | giannoccarol/android_kernel_mmo_msm8994 | `lineage-18.1-cityman-camera` |
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
- Camera: angler 8.1 daemon stack and the IMX230 probe library in the
  vendor tree (`CAMERA-CITYMAN.md`). The rear camera is listed; the sensor
  does not stream yet.
- `boot_cpus=0-5` is kept from talkman: six of eight cores online, the
  setting verified on this phone. All eight cores are untested.

## Not in these repositories

Install these from the phone's own files or the project's local copies:

- Wi-Fi: `qwlan30.bin` (Android SGMT), the Lumia `bdwlan30.bin` board
  calibration and `wlan/qca_cld/WCNSS_qcom_cfg.ini`.
- Bluetooth: `nvm_tlv_3.*.bin`, `rampatch_tlv_3.*.tlv`.
- ADSP firmware for calls: still being changed; see
  `kernel/mmo/msm8994/Documentation/arm64/cityman-adsp.txt`.

## Not verified

No ROM has been built from this tree yet. The AP sensors HAL added to the
talkman tree after the installed ROM, and the qcom sensors blobs listed in
the vendor tree, have not been tried on cityman.
