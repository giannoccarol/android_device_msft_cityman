# Android 13 diagnostic profile at 24 MHz

`lineage-20-cityman-camera` now builds two sensor profiles with the same source
plugin and independently tested angler ABI. The existing `smia9` profile remains
the default. Select `nubia24` explicitly for the next transmitter comparison.

| Setting | smia9 | nubia24 |
| --- | --- | --- |
| MCLK / EXTCLK | 9.6 MHz / 0x0999 | 24 MHz / 0x1800 |
| Output | 2496x1872 RAW10 | 5344x4016 RAW10 |
| Line / frame length | 6024 / 2550 | 6024 / 4150 |
| VT / output clock | 460.8 / 460.8 MHz | 600 / 568 MHz |
| Maximum FPS | 29.99766 | 24 |
| CSIPHY settle | 22 | 27 |
| CSID LUT | CID0 RAW10 | CID0 RAW10, CID1 DT0x36, CID3 embedded |

Both use four lanes, lane assignment 0x0423, CSIPHY0/CSID0, CCI address 0x20,
the measured Cityman power sequence and no shared LVS1/VIO vote. VFE allocation
is left to the coherent angler resource manager; the old A11 selection of VFE0
does not justify forcing it or treating VFE1 as the cause of link ECC errors.

The new profile uses an unmodified 413-write init and 173-write mode 0 extracted
from the private, hash-pinned reference. A software reset precedes init; stream
start/stop and exposure callbacks remain the authored source implementation.
It does not load the old Nubia plugin, daemon, HAL or Android 6 compatibility
libraries. It does not reproduce unlogged dynamic writes from the A11 session.

The CID1/CID3 entries configure the receiver to recognize the auxiliary packet
types seen in the old log; the plugin still exposes one RAW image stream.
No PDAF algorithm, embedded-data parser, AF/OIS or flash support is advertised.
GBRG is the Cityman choice after the on-device Bayer comparison; the Nubia
module's different Bayer enum is not substituted for that measurement.
Existing angler IMX377 chromatix remain provisional ISP tuning, not Lumia
calibration.

## Build the modules in isolation

### Recorded verification and handoff, 2026-10-26

Both profiles passed the ARM32 behavioral tests and the real angler consumer
tests under QEMU, including capability geometry, register/power translation,
all selected CSID pointers and the zero-mode negative control. The unchanged
`smia9` standalone binary still has SHA-256
`5c4fa185e3a354f85cc138e6a6c0dd086828994dd2eb5ba8b13b1800a7a6135c`;
the new standalone `nubia24` test binary is
`4f6905d019e4c23a6774f4e947f5307a0a8a827114aa0a564e0e2501e0f28228`.
These are freestanding test shared objects, not Android production artifacts.
Shell syntax checks and the existing Parcel patch's reverse/applicability
check on the current Android 13 tree passed.

An isolated Android build was started from device `cab103b`, kernel
`037f8741269`, vendor `257aae2` in
`android-cityman/camera-a13-nubia24-20261026`, then stopped at the owner's
request so their main agent can build the combined changes. It did not finish;
there is no verified Android-built `nubia24` artifact or ROM ZIP from this
session. No new profile was installed and no phone operation was performed.

For integration, merge the device/kernel/vendor `lineage-20-cityman-camera`
heads, retain/apply the included Parcel patch, and explicitly select `nubia24`
with the pinned private sensor input. A merge alone leaves `smia9` as default.

Supply the local sensor ELF with SHA-256
`a3f4a7347533fdf04c389d5c328bf22c91cad96901b2629512ca53dc0744490c`.
`generate-nubia24.py` validates it and produces only private register data.
The generated header and input ELF stay outside Git; Android regenerates the
header under `out/` from an explicit input dependency. Wrong/missing input and
unknown profile names fail the build rather than selecting a different mode.

```sh
export CITYMAN_CAMERA_SENSOR_PROFILE=nubia24
export CITYMAN_CAMERA_NUBIA_SENSOR_BLOB=/absolute/path/to/private/libmmcamera_imx230.so
tools/camera-a13/build-hal.sh LOS20 NEW_PRIVATE_BUILD KERNEL_CAMERA VENDOR_CAMERA 6
```

The wrapper snapshots committed camera branches, saves the profile and private
input hash, copies the existing Android output using reflinks, and mounts the
main source tree read-only. Kernel/vendor dependencies remain the matching
Android 13 camera branches. `run-hal-build.sh` resumes that saved profile.

For local ARM32 tests use the same environment variables with `test.sh` and
`test-consumer.sh`. When testing an existing production `.so`, select the profile
matching that binary. Tests verify geometry, both PLL-derived clocks, requested
link rate, EXTCLK/MCLK, exposure bounds, power steps and CSID translation by the
real angler consumer. They do not establish that the sensor emits valid frames.

## Build a complete ROM

Merge/select this device branch and the matching kernel/vendor camera branches
in the ROM source tree before building. No helper resets Git checkouts.
The required framework Parcel patch is included under
`tools/camera-a13/patches/0001-frameworks-av-camera-parcels-android13.diff`;
it is the existing manifest patch, not a new framework implementation.
If absent, review and apply from the Android source root:

```sh
git -C frameworks/av apply -p3 --check "$DEVICE_CAMERA/tools/camera-a13/patches/0001-frameworks-av-camera-parcels-android13.diff"
git -C frameworks/av apply -p3 "$DEVICE_CAMERA/tools/camera-a13/patches/0001-frameworks-av-camera-parcels-android13.diff"
```

Inside the configured Android 13 build container, from `/src`:

```sh
CITYMAN_CAMERA_SENSOR_PROFILE=nubia24 \
CITYMAN_CAMERA_NUBIA_SENSOR_BLOB=/mounted/private/libmmcamera_imx230.so \
JOBS=6 bash device/msft/cityman/tools/camera-a13/build-rom-profile.sh
```

The wrapper verifies the Parcel patch is already applied, validates/stages the
private input under `out/`, installs the source module through
`CITYMAN_CAMERA_SMIA_EXPERIMENTAL=true`, and builds `bacon`. It does not flash.
For the baseline build select `CITYMAN_CAMERA_SENSOR_PROFILE=smia9` instead.

## First phone comparison and rollback

The kernel caches the power sequence from the first successful sensor probe.
Replacing the plugin after a 9.6 MHz probe in the same boot does not guarantee
MCLK changes to 24 MHz. Have the intended profile present before the first
probe of a fresh boot; do not combine a live module swap with an assumed clock
change. The generic kernel accepts the plugin's 24 MHz request without the old
global DT clock override. Coordinate reboot/install with the owner.

Keep the current boot/ROM and source plugin hash before installing the next
build. The baseline Android plugin hash is
`849a6f2365f3b6fad30859c771694bb9ddab93fa8a4f76b31730eba9c40a14bd`.
Rollback means restoring the matching baseline ROM/module and booting with
`smia9` present from the first probe; do not restore an Android 11 boot image.

Record the build source manifest, installed plugin hash, actual MCLK, probe ID,
CSID lane/DT/decode configuration, ECC/CRC, ISPIF/VFE SOF and HAL buffer delivery.
Accept a real preview, then repeatable open/close and a saved image. Packet
counts, enumeration or passing offline tests alone do not establish success.

`tools/camera-a13/collect-camera.sh ADB_SERIAL NEW_PRIVATE_OUTPUT` collects
bounded read-only logs, installed plugin hash, available debug clock readings
and camera-service state. It neither opens an app nor restarts any service.
Run it before and after a coordinated test, with separate private directories.
An idle/unavailable debug clock reading does not verify the active sensor clock.

## Credits

- TheMuppets, `proprietary_vendor_zte`, `cm-13.0`, commit `010f45bac6a3`:
  https://github.com/TheMuppets/proprietary_vendor_zte . Private reference
  register programs only; no vendor input or generated table is committed.
- hraj9258, `android_kernel_mmo_msm8994`, commit
  `6ab0e0e9018d6185b642fa86de8b1c61cbd2701b`, baseline SMIA profile:
  https://github.com/hraj9258/android_kernel_mmo_msm8994 .
- archienz, Cityman/Talkman lane and clock findings; device tree `e33ff2c731ea`:
  https://github.com/archienz/android_device_msft_talkman .
- Google angler driver `309d642245e2` and OPM7.181205.001 private ABI fixtures;
  Android4Lumia950 (djokcho, EpicLPer, j0sh1x and community), LineageOS and
  LineageOS-UL for the inherited kernel, HAL and framework integration.
- The owner integrated the 32-bit provider and SensorManager dependency fix
  (`0cc98f5`, `25a6602`, vendor `257aae2`) and the existing framework Parcel
  patch from `giannoccarol/talkman-manifests` `74cd8cb`. It resolves the
  LineageOS-UL framework/service layout mismatch described in the main report.
