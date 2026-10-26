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
A new source IMX230 SMIA++ library now lives in `camera-sensors/imx230`.
It passes ARM32 behavioral and actual angler consumer interoperability tests,
including the Android-built shared object. Installation remains opt-in through
`CITYMAN_CAMERA_SMIA_EXPERIMENTAL=true`; no working-camera claim is made yet.

## Sensor integration boundary

The next rear mode is the SMIA++ uncompressed RAW10 2496x1872 program published
by hraj9258. It uses a 9.6 MHz external clock, four lanes and CSI0. Its sensor
metadata, output geometry, exposure callbacks and start/stop sequence must be
implemented together in a module matching the selected angler sensor ABI.
Selecting a different name for an IMX377 binary does not provide that module.

The published kernel references `senslib/sensor_lib_imx230.c`, but that original
file was not found. The owner requested a new equivalent source module with a
verified interface. `camera-sensors/imx230/ABI.md` documents the independently
specified plugin boundary, GPL kernel records, pinned official angler fixtures
and tests. It does not import confidential sensor SDK headers or edit a sensor
binary. Ordinary pointer-based register tables carry the sensor programs;
MAX_I2C_REG_SET=12 applies to a different inline register record.

Front Ducati stays disabled in the kernel until its own sensor programming and
library exist. Module ID 0x2140 and die ID 0x03bb are measurements; they do not
establish a specific commercial part number or a working front camera.

## Validation and phone tests

### Latest main-branch hardware results (2026-10-26 evening)

The owner merged the camera work into `lineage-20-cityman` and tested the
Android 13 v8/v9 builds. The camera worktrees now include device `1f2df92`,
vendor `257aae2` and kernel `037f8741269`. This includes the 32-bit hwbinder
provider, `libsensor_vendor` compatibility dependency and the single-rild fix.
The temporary raw_depth change to 2 was reverted: 1 remains correct for the
sensor output descriptor; CSI's decode enum separately remains 2 for RAW10.

The source sensor probes and Android lists the rear camera. The owner's
framework parcel fix permits session configuration and stream start. A saved
kernel log records about 300,000 CSI packets in 5.3 seconds, but header ECC
counter 0xffff0000 and no ISPIF/VFE SOF; no preview or image is verified.
Changing settle from 22 to 27 did not fix it. The current MCLK is 9.6 MHz,
matching EXTCLK 0x0999 and the pinned SMIA mode. A clock-only change to 24 MHz
would also alter the PLL outputs; it is not an equivalent configuration.

`camera-sensors/imx230/inspect-nubia-registers.py` now extracts the old private
Nubia reference offline, validating its SHA-256, ARM32 ELF LOAD mappings and
relative relocations for the pointer-based register records. It finds the
413-write init and nine mode tables. Mode 0 has 173 writes, 5344x4016 output,
line length 6024 and frame length 4150, matching the old A11 log's counts and
geometry. This does not reconstruct dynamic exposure writes or prove that
every static write reached the sensor during the old session.

The reference writes EXTCLK 0x1800 and explicit D-PHY timing at 0x080a--0x0819;
the SMIA programs do not write that timing range. This is a comparison target,
not proof that missing writes cause the ECC errors. The different clock/link
profiles prevent blindly copying those values. Keep one variable at a time:
first establish a coherent sensor clock/PLL/D-PHY program, then evaluate ECC,
ISPIF/VFE SOF and actual delivered frames. Preserve the Android 13 HAL/daemon
interface while comparing sensor programming.

```sh
python3 camera-sensors/imx230/inspect-nubia-registers.py PRIVATE_NUBIA_SO \
  > PRIVATE_OUTPUT/nubia-vs-smia.json
```

The JSON contains extracted vendor register data and stays outside Git; no
vendor table is incorporated into the GPL production source. Original logs
and comparison output are in the surrounding project, respectively
`dumps/a13-v8cam-20261026`, `dumps/trial-nubia-hal-v6-153433`, and
`builds/camera-mipi-comparison-20261026`. ADB reappeared after the offline
comparison: a read-only check confirmed kernel `g037f8741`, production module
SHA-256 `849a6f2365f3b6fad30859c771694bb9ddab93fa8a4f76b31730eba9c40a14bd`,
running daemon/provider, and one camera. The current kernel log also reports
300,748 packets with ECC 0xffff0000 and CRC 0. CameraService's dump timed out;
the partial output is preserved. No services were restarted or phone files
changed during this check.

### Earlier isolated preparation

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

### Source module and updated baseline

The kernel and device camera branches were merged with the owner's current
`lineage-20-cityman`: kernel through `85514ba37fc` (including audio calibration
changes) and device through `fb0314c` (including Bluetooth/Wi-Fi/audio changes).
The source-module build used device `4b6259b`, kernel `dbef37116b8` and vendor
`8e70346`, with the main ROM tree mounted read-only and a private output.

- Kernel `dbef37116b8` rebuilt successfully; Image.gz-dtb SHA-256
  `7ffa4d330e39624897bbadbfbd1b67c16fa8a7691b3c1695c7123d7b69a625de`.
- Android-built `vendor/lib/libmmcamera_imx230.so` SHA-256
  `849a6f2365f3b6fad30859c771694bb9ddab93fa8a4f76b31730eba9c40a14bd`.
- Actual angler consumer loaded that Android shared object under QEMU, read
  2496x1872 capabilities/GRBG10, translated I2C/power/CSI records, and rejected
  a zero-mode negative control. Android packed relocations were decoded by the
  offline fixture loader; no alternate production ELF was substituted.
- Separate ARM32 tests exercise exposure/gain limits, invalid inputs, group
  hold/release and register-buffer canaries. The register header regenerates
  exactly from the pinned public SMIA++ source.

Only build/test wrappers and documentation changed after those source snapshots.
The new library remains an experimental sensor implementation: actual image
delivery, analog programming at the unchanged 0x20 address, shared VIO ownership,
tuning, optical metadata and repeated power/stream cycles need hardware tests.
Existing angler IMX377 chromatix are an ISP seed, not Lumia colour calibration.

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
- TheMuppets, private comparison input for the Nubia NX510J IMX230 programming,
  `proprietary_vendor_zte` branch `cm-13.0`, commit `010f45bac6a3`:
  https://github.com/TheMuppets/proprietary_vendor_zte . Input SHA-256
  `a3f4a7347533fdf04c389d5c328bf22c91cad96901b2629512ca53dc0744490c`.
  The inspection tool is new code; the vendor programs remain private outputs.
