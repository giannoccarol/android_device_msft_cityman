# Cityman camera on Android 13

This branch starts at `lineage-20-cityman` commit
`ce7ef2c79eea8a853650399fc4d4f9c45d05514d`. It keeps that tree's source-built
QCamera2 HAL, Android 13 provider and media interfaces. It does not import the
Clark HAL or the earlier Android 6 binary wrappers.

The kernel and vendor dependencies both select `lineage-20-cityman-camera`.
Those branches are local until explicitly published. Use local worktrees for
builds in the meantime; fetching these branch names from GitHub will not work.

The camera kernel starts at Android 13 commit `5827d9a6f12` and brings in the
angler camera driver plus Cityman wiring. The vendor keeps the complete angler
MSM8994/VFE 4.6 daemon set. It removes the old IMX377-derived IMX230 probe module.
A matching Lumia sensor library has not yet been supplied or implemented.
Consequently this baseline is not expected to expose a working camera.

## Sensor integration boundary

The next rear mode is the SMIA++ uncompressed RAW10 2496x1872 program published
by hraj9258. It uses a 9.6 MHz external clock, four lanes and CSI0. Its sensor
metadata, output geometry, exposure callbacks and start/stop sequence must be
implemented together in a module matching the selected angler sensor ABI.
Selecting a different name for an IMX377 binary does not provide that module.

The published kernel references `senslib/sensor_lib_imx230.c`, but that file is
not present in the inspected public kernel or device branches. The kernel's
mode programs alone cannot supply the missing userspace contract. The owner
has no separate package/link. Do not turn this unknown into another binary
layout patch or a global ISP-register override.

Front Ducati stays disabled in the kernel until its own sensor programming and
library exist. Module ID 0x2140 and die ID 0x03bb are measurements; they do not
establish a specific commercial part number or a working front camera.

## Validation and phone tests

Kernel compilation passed (source `d62fca264ee`, image hash in the kernel report).
The HAL and its three requested interface/JPEG modules also passed an isolated
Android 13 build, using device `261de03`, kernel `d62fca264ee`, vendor `8e70346`.
The private build folder is `android-cityman/camera-a13-hal-20261026`; its
`source-commits.txt`, `build.log` and `camera.SHA256SUMS` identify the inputs
and outputs. Later changes only improve the build wrapper and documentation.

The initial wrapper lacked a writable compiler cache; that was fixed with a
private `/ccache` mount before the successful build. The output hash list was
then corrected to use the source JPEG interface's actual vendor install path.
Neither repair changes camera product code or binaries.

No flash, service restart or sensor write is part of this preparation.
The owner is handling Android 13 reboot problems independently.

When the matching sensor module exists, the first hardware acceptance is a real
image delivered through the HAL, then repeatable preview/open/close and a saved
JPEG. CSI packet counts or a listed camera ID alone do not pass that gate.
Flash, autofocus/OIS, full resolution and front capture need separate evidence.

## Credits and sources

- Android4Lumia950 (djokcho, EpicLPer, j0sh1x and community), original Lumia
  device/kernel base: https://github.com/Android4Lumia950/android_device_msft_talkman
- LineageOS and LineageOS-UL, QCamera2 and Android 13 integration; inherited HAL
  compilation fixes `307c85b` and `57d7b3f` on this repository's Android 13 base.
- Google angler camera driver, `android-msm-angler-3.10-oreo-m7`
  `309d642245e2`; the matching vendor is from factory image OPM7.181205.001.
- archienz, Talkman lane assignment 0x0423 and sensor identification,
  device tree `e33ff2c731ea`: https://github.com/archienz/android_device_msft_talkman
- hraj9258, SMIA++ mode/clock work, kernel `talkman-cam-clean`
  `6ab0e0e9018d6185b642fa86de8b1c61cbd2701b`:
  https://github.com/hraj9258/android_kernel_mmo_msm8994/tree/talkman-cam-clean
