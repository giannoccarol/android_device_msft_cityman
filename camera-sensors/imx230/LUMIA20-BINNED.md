# lumia20: opt-in 2x2-binned 30 fps mode

`CITYMAN_CAMERA_LUMIA20_BINNED=true` adds a second mode to the `lumia20`
profile: 2672x1872 RAW10, 2x2 binning of the full 5344x3744 readout, 30 fps.
The full-resolution 20 fps mode stays first and unchanged. The flag is off by
default, and with it off the module is byte-identical to the previous source.

**Status (2026-10-31): offline only.** Register derivation, frame timing,
descriptor ABI and the angler daemon's own mode selection are checked on the
host. Nothing has run on the phone.

## Why

Preview and video want 30 fps; the full readout gives 20. A binned pixel
combines four photosites, so the binned stream is also cleaner in low light
(at least 2x SNR if the sensor averages, more signal if it sums; to measure).
Stills keep the full mode.

## The two modes

| | mode 0 (full) | mode 1 (binned) |
| --- | --- | --- |
| Output | 5344x3744 | 2672x1872 |
| Readout window | x 0-5343, y 136-3879 | same |
| Binning (0x0900/0x0901) | off / 1x1 | on / 2x2 |
| MCLK, PLL, link | 9.6 MHz, 1190.4 Mbit/s per lane | same |
| VT / output clock | 476.16 / 476.16 MHz | same |
| Line length | 6024 (12.651 us) | same |
| Frame length | 3952 | 2634 |
| Frame rate | 20.0006 fps (advertised 5-20) | 30.0087 fps (advertised 7.5-30) |
| Line on the link | 53440 of about 60240 bit (89 %) | 26720 bit (44 %) |
| CSI | 4 lanes, lane_assign 0x0423, settle 22 | same |

Same field of view as the active array (5344x3744, unchanged), so crop and
3A regions map to mode 1 by a plain factor 2. Same PLL and lane rate, so the
CSIPHY settle measured for lumia20 still applies and a mode switch does not
change any clock. `binning_factor` is 2 for mode 1.

Frame length: 476.16 MHz / (6024 x 30) = 2634.8; 2634 gives 30.009 fps. The
daemon only selects a mode for a 30 fps request if its maximum fps is at least
30.0 (see below), so 2635 (29.998) would not work. Vertical blanking is 762
lines (9.6 ms); the longest exposure at 30 fps is 2626 lines (33.2 ms).

## Register derivation

`generate-lumia20.py` emits the binned table as a macro only: Lumia mode 0
(73 writes from the private DCC) with these 13 writes changed, in place:

| Register | Value | Meaning |
| --- | --- | --- |
| 0x034C/0x034D | 0x0A70 | x_output_size 2672 |
| 0x034E/0x034F | 0x0750 | y_output_size 1872 |
| 0x040C/0x040D | 0x0A70 | digital crop width |
| 0x040E/0x040F | 0x0750 | digital crop height |
| 0x0900 | 0x01 | binning on |
| 0x0901 | 0x22 | 2x2 |
| 0x3A25/0x3A26 | 0x0750 | vendor mirror of y_output_size |
| 0x3A38 | 0x01 | vendor mirror of the binning factor |

Everything else (PLL 0x0300-0x030F, link rate 0x0820/0x0821, window
0x0344-0x034B, analog width mirror 0x3A23/0x3A24 = 5344) stays as in mode 0.
The resolution tables also carry their frame length (0x0340/0x0341), since
the init value of 3952 would otherwise survive a mode switch.

The derivation is checked three ways:

1. The generator refuses to emit unless the result equals an independent
   composition from the same DCC: the Windows full-width binned table
   (2672x1504, offset 0x0C6D) with mode 0's PLL/link and the vertical window
   of the Windows 1872-row binned table (offset 0x0D4B).
2. The first 17 DCC mode tables share one register order and differ only in
   geometry, binning, PLL and RAW10/RAW8 output. Windows also runs binned
   modes at 1190.4 Mbit/s per lane, with a faster pixel clock than here
   (offsets 0x11A1, 0x1519 and 0x15F7: 595.2 MHz, 8-bit output).
3. The Nubia IMX230 tables (24 MHz, private, reference only) differ between
   their full and their 30 fps 2672x2008 binned mode by exactly these
   registers plus frame length, exposure and the PDAF window (0x315x-0x31xx,
   unused here), with the same PLL and line length 6024.

## How the angler daemon picks the mode

From `libmmcamera2_sensor_modules.so` (pinned fixture, exported
`sensor_pick_resolution`, tables `pick_v1`/`pick_v3`, `check_v1`). Its code is
identical in the A16 vendor copy, which differs only by a sensor-name string.

- The HAL sends the session's largest stream size and the requested fps.
  HAL version 3 selects `pick_v3`. Any session with a preview stream is the
  "video/preview" use case.
- For that use case `pick_v3` enables four checks: max fps in
  [requested, requested + 20) (requested 0 always passes), width >= requested,
  height >= requested, and smallest area among the modes that pass.
- Modes are tried from the last index down. If none passes, index 0 is used.
  So mode 0 must be the full mode.

What this gives (also asserted in `test-consumer.sh` with the daemon code):

| Session (largest stream, fps) | Mode |
| --- | --- |
| Photo: full-size JPEG + preview, 30, 20 or unset | 0 (30: no match, fallback; capped to 20 fps) |
| Video 1920x1080 at 30, 24 or 15 | 1 |
| Preview only, 1440x1080 at 30 | 1 |
| Anything wider than 2672 at 30 | 0 (fallback) |

So recording video and small-still sessions get 30 fps. A photo session with
a full-size JPEG stream stays at 20 fps, as today: HAL3 sets one sensor mode
per session, from the largest stream.

## Why smia9 never gave an image

1. **Lane order.** `smia_idle` sets 0x50B8..0x50BB = 2,1,3,0 (the Windows
   lane routing), and the CSID already reorders the lanes (0x0423). The two
   permutations stacked and every packet header failed ECC, so no SOF. This was
   bisected on the phone for lumia20 on 2026-10-27; smia9 uses the same init.
   The current source resets the order for smia9 too, but smia9 has not been
   tested on the phone since.
2. **29.99766 fps.** In a 30 fps session the daemon logged `check fps: 0` /
   `Can't match` for smia9 and fell back to index 0 (its only mode), capping
   the rate. Harmless with one mode; in a two-mode table it would never be
   picked for 30 fps. Hence 2634 lines here.
3. smia9 is a 4:3 crop (x 176-5167). As a second lumia20 mode it would not
   cover the active array, so mode 1 bins the full window instead.

The PLL (1152 Mbit/s per lane) and the table itself are not suspect: the
first 73 smia9 writes equal the Windows DCC binned mode at offset 0x0D4B.

## Build and host tests

```sh
# Android: add to the lumia20 environment (Android.mk refuses other profiles)
CITYMAN_CAMERA_LUMIA20_BINNED=true

# Host (no phone, no Android build)
export CITYMAN_CAMERA_SENSOR_PROFILE=lumia20 CITYMAN_CAMERA_AF=true \
       CITYMAN_CAMERA_LUMIA20_DCC=/path/to/private/0AEACA05.dcc
CITYMAN_CAMERA_LUMIA20_BINNED=true ./test.sh KERNEL ARM_GCC_PREFIX OUT
CITYMAN_CAMERA_LUMIA20_BINNED=true ./test-consumer.sh KERNEL ARM_GCC_PREFIX ANGLER_VENDOR_LIB OUT
```

`test.sh` now checks, for every mode: output, crop and window sizes against
the binning registers, PLL clocks, link-rate register, frame length, the fps
the clocks give against the advertised maximum, the per-line MIPI budget, CSI
and tuning records. It also checks that the modes share PLL, link and window,
that mode 0 is the full readout, and that `fill_exposure` keeps each mode's
frame length. `test-consumer.sh` loads the module into the real angler
consumer, checks the exported mode list and the active array, translates
every CSI record, and runs the daemon's `sensor_pick_resolution` on the
sessions in the table above (`pick_v3` and the default `pick_v1`).

Recorded on 2026-10-31 with the A16 `arm-linux-androideabi-4.9` GCC and
qemu-arm: both tests pass for smia9, nubia24, lumia20 and lumia20 binned.
The platform clang (r574158) builds lumia20 with and without the flag with
`-Wall -Wextra -Werror`. The default smia9, nubia24, lumia20 and lumia20+AF
test builds are byte-identical to those from the previous source. These tests
had gone stale (lane_order init entry, active array, Bayer enum 3); they were
updated to the shipped values. Mutations caught: 2640 lines (29.94 fps),
max fps 29.99, binning factor 1, and wrong binning or mirror deltas (the
generator stops).

## Phone test (to do, with the owner's go-ahead)

MCLK does not change, so the module can be swapped by bind mount without a
reboot (camera services restarted).

1. Build only the module, when no `los23-build-*` container is running, from
   a `los23` tree whose `device/msft/cityman` has this commit:

   ```sh
   cd android-cityman/build-env-l23
   TARGET="libmmcamera_imx230 CITYMAN_CAMERA_LUMIA20_BINNED=true" ./run-build.sh build-rom.sh
   # out/target/product/cityman/vendor/lib/libmmcamera_imx230.so (private: DCC data)
   ```

2. Install as a bind mount (`adb root`):

   ```sh
   adb push libmmcamera_imx230.so /data/local/tmp/imx230-binned.so
   adb shell 'stop qcamerasvr
     chcon u:object_r:vendor_file:s0 /data/local/tmp/imx230-binned.so
     mount -o bind /data/local/tmp/imx230-binned.so /vendor/lib/libmmcamera_imx230.so
     start qcamerasvr; setprop ctl.restart vendor.camera-provider-2-4
     sleep 2; setprop ctl.restart cameraserver'
   adb shell am force-stop org.lineageos.aperture
   ```

3. Check, with `adb logcat -b all` and `dmesg`:
   - `dumpsys media.camera`: expected fps ranges include (7.5, 30), and
     minimum frame durations of 33.3 ms for 1920x1080 and 50 ms for
     5344x3744 (how the HAL derives them is not checked offline);
   - photo mode: `pick_w:5344, pick_h:3744`, preview as today;
   - video mode (1080p): `Matched pick_w:2672, pick_h:1872, pick_fps:30`,
     `sensor_get_output_info ... pick res 1`, no `set_fps ... capping`;
   - CSID: `ecc 0`, no CRC errors, SOF on the VFE; no `camif error`
     (the CAMIF must see 2672 pixels per line), no `SOF freeze`;
   - frame rate: recorded video at about 30 fps (`ffprobe`), preview
     timestamps 33 ms apart;
   - image: colour, black level in a dark frame (binning may change the
     pedestal), noise compared with mode 0, AF, switching photo to video
     and back several times.
4. Roll back: `adb shell 'stop qcamerasvr; umount
   /vendor/lib/libmmcamera_imx230.so; start qcamerasvr; setprop ctl.restart
   vendor.camera-provider-2-4; sleep 2; setprop ctl.restart cameraserver'`,
   or reboot (the mount does not survive it).

## Limits and open points

- No phone test: streaming, link, image and fps are unverified.
- Both modes use the same chromatix (made for the full readout); noise
  reduction and black level for the binned stream may need their own values.
- `binning_factor` 2 may be read by AEC; the Google IMX377 plugin leaves its
  binned HFR modes at 1. One-line change if exposure or ISO looks wrong.
- Minimum 7.5 fps lets preview slow down in the dark; 15 is the usual choice
  if that is not wanted.
- Photo sessions with a full-size JPEG stay at 20 fps (one mode per session).

## Credits

- hraj9258 (Himanshu Raj), talkman camera port, public SMIA++ tables and clock
  research: https://github.com/hraj9258/android_kernel_mmo_msm8994,
  `talkman-cam-clean` at `6ab0e0e9018d6185b642fa86de8b1c61cbd2701b`. smia9's
  binned table is his; this mode follows the same register scheme.
- archienz, Talkman lane map 0x0423, 24 MHz MCLK and CCI findings:
  https://github.com/archienz/android_device_msft_talkman, `e33ff2c731ea`.
- Android4Lumia950 (djokcho, EpicLPer, j0sh1x and the community), Lumia base.
- Microsoft Lumia 950 XL camera calibration (private DCC, not committed) and
  the Nubia IMX230 tables (private, comparison only; TheMuppets
  `proprietary_vendor_zte` `010f45bac6a3`).
- Google angler OPM7.181205.001 camera daemon, used only as a private test
  fixture for the mode-selection analysis.
