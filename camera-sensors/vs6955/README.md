# ST VS6955CA front camera (Cityman "Ducati")

Experimental source sensor module for the Lumia 950 XL front camera. It is
**off by default** and has not been tested on hardware yet.

## Identification

- Sensor: STMicroelectronics **VS6955CA**, SMIA++, module name **Ducati**.
- Die ID `0x03BB` at register `0x0016`; manufacturer `0x01` at `0x0019`.
- NVM module ID `0x2140`, selecting the stock DCC `0A214000`/`0A214001`.
- I2C address `0x20`, 16-bit register addresses, CCI master 0.
- CSIPHY2 / CSID2, **MCLK2 9.6 MHz**, reset GPIO 104, standby GPIO 105.
- VANA = PM8994 **L17 2.8 V**, VIO = PM8994 **LVS1 1.8 V**; no VAF rail.
- **Fixed focus**: no actuator, OIS or AF data; do not add one.

The current kernel device tree already describes CSI2, MCLK2, the GPIOs and
the rails, but `qcom,camera@1` is `status = "disabled"`. The front plugin
only needs a probe library and the sensor-list entry before that node can be
enabled.

## Private input

The register programs come from the stock Lumia FFU 10586.13169 CDCC
`0A214000.dcc`:

```
sha256 16cc9406ad86f149115181596bd62fb0eb7231ede319df15bfe53575a26be10c
```

The DCC is proprietary and is **not** part of this tree. `generate-vs6955.py`
validates that hash and emits `vs6955_registers.generated.h` under the build
output directory. The extracted init is 805 writes (`0x385..0xCF1`), including
the `0xF800..0xFAB5` firmware patch; the eight mode tables are at `0xD00`
(4 resolutions times 2 PLL groups). The first Android profile uses the binned
`1296x972` table as the initial low-bandwidth mode.

## Build

The module is built only when explicitly enabled:

```sh
export CITYMAN_CAMERA_FRONT_VS6955=true
export CITYMAN_CAMERA_FRONT_DCC=/absolute/path/to/0A214000.dcc
m libmmcamera_vs6955
```

The private DCC and generated header stay outside Git. The result is
`/vendor/lib/libmmcamera_vs6955.so` (32-bit vendor module).

## Phone test plan

1. **Probe**: power L17/LVS1, MCLK2 9.6 MHz, reset 104, standby 105; read
   `0x0016 == 0x03BB`.
2. **Sensor list**: the daemon's list is compiled in
   `libmmcamera2_sensor_modules.so` and currently contains `imx258`, not
   `vs6955`. A private same-length replacement is in
   `tools/camera-a13/patch-sensor-list.py`; use it on a copy and bind-mount or
   install the patched library.
3. **Device tree**: apply `front-enable-camera1.patch` and build a kernel
   with the front node enabled.
4. **Stream**: start with the binned 1296x972 mode; verify CSID2 SOF/EOF and
   a delivered frame.
5. Only after that: full 2592x1944, 16:9, exposure, Bayer, colour tuning.

## Not verified yet

- CSI lane assignment (`0x3210` provisional) and settle count (27 provisional).
- Frame length (1125), VT/output clock (92.8125 MHz) and FPS (30) are derived
  from the line length and a 30 fps assumption; they must be measured.
- SMIA++ exposure/gain mapping and the firmware patch timing.
- Per-unit OTP/calibration (Windows reads the white balance of each module
  from its NVM; Android does not), lens shading and AWB reference points.
- There is no autofocus and no flash for this sensor.

## Credits

- Microsoft/Lumia stock FFU build 10586.13169 and the LumiaWoA driver pack:
  the front DCC (register programs and the colour matrices of its
  `ISP_HW_QC` and `ISP_SW_ARC` blocks), SMIA++ driver and ACPI wiring
  reference.
- The reverse-engineering work and extracted tables are kept in the project
  private evidence directory; no vendor table is committed here.
- The angler SMIA++ sensor ABI and the Cityman device tree are inherited from
  Android4Lumia950, LineageOS/LineageOS-UL, archienz and hraj9258 as recorded
  in the parent camera reports.
- **hraj9258** (Himanshu Raj), talkman port
  ([gitlab.com/hraj9258/android_device_msft_talkman](https://gitlab.com/hraj9258/android_device_msft_talkman),
  `camera/sensors/imx230` at `901b039`): the chromatix rebuilt in C with the
  module's own DCC colour matrix in every entry and its measured black level
  (`../rear-tuning`), the approach the front options below follow.
- **Google / Qualcomm**: the angler imx179_liteon tuning the front chromatix
  still uses as donor.

## Measured on Android 16 (31/10/2026)

- Bayer order `2` (`.output` last field): orders 0 and 3 gave an almost
  colourless picture (3 was the old black-and-white front camera), 1
  swapped red and blue.
- Blue digital gain 1.35x (`0x0212/0x0213`) neutralises the yellow cast of
  the borrowed IMX179 chromatix (1.20x still warm, 1.50x blue).
- The front sensor streams at most 1280x960; its camcorder profiles stop at
  720p (`configs/media_profiles.xml`).

## Colour options (7/11/2026)

The front borrows all of its tuning from the angler IMX179 (Nexus 6P front):
colour matrix, AWB reference points, lens shading and black level. A fixed
blue digital gain of 1.35x on the sensor hides the resulting yellow cast, and
AWB barely moves (on a white wall the output B/G follows the sensor gain one
for one). These options make the parts that can be fixed without new
measurements selectable, so they can be compared on the phone. Every default
except the colour matrix is the 31/10 behaviour.

| Option | Build variable | Property (read when the daemon loads this module) | Default |
|---|---|---|---|
| Colour matrix set | `CITYMAN_CAMERA_FRONT_CCM` = `sw_arc`, `hw_qc`, `imx179` | `vendor.camera.front.ccm` = `swarc`, `hwqc`, `imx179` | `sw_arc` |
| Which DCC matrix | `CITYMAN_CAMERA_FRONT_CCM_INDEX` = `mean`, `0`..`5` | none (rebuild) | `mean` |
| Black level | `CITYMAN_CAMERA_FRONT_BLACK` = `V` or `R Gr Gb B` (12-bit) | `vendor.camera.front.common` = `vs6955`, `imx179` | empty: the vendor imx179 common |
| Blue digital gain (`0x0212/0x0213`) | `CITYMAN_CAMERA_FRONT_BLUE_GAIN` = code, `0x100` = 1.0x | `vendor.camera.front.blue_gain` = code (decimal or `0x` hex) | `0x0159` (1.35x) |

What each one does:

- **Colour matrix.** `../front-tuning` builds the three VFE chromatix the
  front loads (preview/snapshot, video, liveshot) in three sets:
  `libchromatix_vs6955_imx179_*` keeps the donor's eight matrices and is
  byte-identical to the 31/10 `libchromatix_vs6955_*`;
  `libchromatix_vs6955_swarc_*` and `libchromatix_vs6955_hwqc_*` put one
  matrix from the DCC in all eight entries, so the chromatix interpolation
  cannot blend the IMX179's back in. The DCC holds six VS6955 matrices (Q16,
  rows summing to 1) in two blocks: `ISP_HW_QC` ("Mid tuning") and the softer
  `ISP_SW_ARC` ("Final tuning"). Which illuminant each of the six belongs to is
  not known, so the default is their average (`mean`); `0`..`5` picks one.
  `gen-front-ccm.py` reads the hash-checked DCC during the build and writes the
  matrix to a header in the build output: no DCC-derived number is in this
  tree. All three sets are always installed; the property switches between
  them, the build variable picks the default.
- **Black level.** The donor's linearization starts at 256/257 (12-bit),
  IMX179's pedestal; the VS6955's is unmeasured (on the rear the magenta cast
  came exactly from a wrong black level). With `CITYMAN_CAMERA_FRONT_BLACK`
  the build adds `libchromatix_vs6955_common`, the donor's common chromatix
  with every knee ladder shifted to start at the given value per channel and
  the segment widths kept (the rear's `lin_shift`). The module then loads it
  by default; `vendor.camera.front.common=imx179` returns to the vendor blob.
  The values are in the chromatix's channel order R, Gr, Gb, B; for this GRBG
  sensor the mapping to the VFE register groups has not been read back, so
  start from one value for all four.
- **Blue gain.** The 1.35x on `0x0212/0x0213` (`503c139`) compensates the
  IMX179 tuning; with a VS6955 matrix and working AWB it should go back to
  1.0x (`0x100`). The property is clamped to `0x100..0xfff`; a malformed value
  is ignored.

Properties are `vendor.*`, so they are lost on reboot: use them for A/B and
the build variables for a release. The daemon reads them only when it loads
the module, so after a change restart the camera stack (`adb root`):

```sh
adb shell 'setprop vendor.camera.front.ccm hwqc
  stop qcamerasvr; start qcamerasvr
  setprop ctl.restart vendor.camera-provider-2-4; sleep 2
  setprop ctl.restart cameraserver'
```

With the front camera open, the loaded chromatix shows which set is active:

```sh
adb shell 'grep -o "libchromatix_[a-z0-9_]*" /proc/$(pidof mm-qcamera-daemon)/maps | sort -u'
```

`../front-tuning/gen-front-chromatix.py verify-built <dir> <dcc>` checks
built libraries against the donor (only the matrix, the AEC targets and the
black level may differ) and names which DCC matrix each set holds, without
printing it.

On a phone with a build from before these options (31/10 names), the matrix
can still be tried without installing: bind-mount a new
`libchromatix_vs6955_swarc_<mode>.so` over `/vendor/lib/libchromatix_vs6955_<mode>.so`
for the three modes, as in `../imx230/LUMIA20-BINNED.md`. The properties
need this branch's `libmmcamera_vs6955.so` together with its chromatix.

## Colour tests on the phone

Front camera, stock camera app, `adb root`. Keep each photo and the log.

**T1, AWB under three lights.** `setprop persist.camera.stats.debug 5`,
restart the camera stack, fill the frame with a white sheet and hold still
10 s under (a) daylight by a window, (b) warm tungsten or halogen, (c) LED or
fluorescent ceiling light. For each, save `adb logcat -d -b all` (the AWB
lines give the decision, the R/G and B/G gains and the CCT) and a photo, and
measure R/G and B/G at the centre of the photo. Repeat with
`vendor.camera.front.blue_gain=0x100`. AWB works if its gains change between
the lights and the sheet stays within about 0.05 of neutral; if B/G still
follows the sensor gain, AWB is stuck and the reference points (not part of
these options) are next.

**T2, black level.** Front lens fully covered, room dark, AE settled. Dump
the RAW10 stream (HAL1: `persist.camera.raw_yuv=1`,
`persist.camera.dumpimg=16`, files in `/data/misc/camera/`) and take the
mean of each of the four Bayer positions (GRBG) over the centre. Multiply
the 10-bit means by 4 and build with `CITYMAN_CAMERA_FRONT_BLACK` set to them
(one value if they agree within 2 counts). Check with a second dark frame
that the processed image is neutral grey, not tinted, and compare with
`vendor.camera.front.common=imx179`.

**T5, colour matrix A/B.** One scene with saturated colours and a white
sheet (a colour chart if available), under the three lights of T1. For each
light take one photo with each `vendor.camera.front.ccm` value (`imx179`,
`swarc`, `hwqc`), restarting the camera stack in between, and the same with
the rear camera as reference. Judge neutrality of the sheet, hue of skin and
of red/green/blue objects and saturation. If one set wins, try
`CITYMAN_CAMERA_FRONT_CCM_INDEX` `0`..`5` against `mean` on the light where
it is weakest (this needs a rebuild of the front chromatix only).

Not verified yet: none of these options has been run on the phone; the
libraries were built on the host and checked against the donor only.
