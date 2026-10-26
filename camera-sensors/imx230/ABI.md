# Source IMX230 SMIA++ module for the angler interface

This is a new sensor plugin, not the missing hraj9258 source file. It is built
from C and register programs; no input sensor binary is patched or copied into
the output. No Qualcomm proprietary/confidential SDK header is included.

The owner requested an equivalent module with a verified interface on
2026-10-26. The separately specified `angler_sensor_abi.h` describes the external
plugin boundary only. Kernel records come directly from the matching GPL
`msm_camsensor_sdk.h`. This does not alter or recreate daemon-internal state.

## Reference and independent checks

The official Google angler OPM7.181205.001 vendor files are private fixtures:

| File | SHA-256 |
| --- | --- |
| `libmmcamera2_sensor_modules.so` | `2f9949e423c8bb65089fb0264a3ab812274efde578a300b789f6626c2e1d6803` |
| `libmmcamera_imx377.so` | `3c1f5d3583f88b3592eaf21afad8bd65b7b054a87ec178c74541f252b19d22bc` |

`sensor_open_lib` returns a descriptor; its kernel slave record occupies 0x61c
bytes on ARM32. The public consumer's capability function at Thumb VA 0x20c89
reads modes at 0xc80 with stride 56 and count at 0xeb0. Its initialization
function at 0x21ff5 reads pointer-based register records at 0xac8, stride 20,
count at 0xb90. Thus MAX_I2C_REG_SET=12 is not a limit on ordinary init/mode
tables. The original reference uses an init table of 93 and mode tables of 74.

The exposure calculation call at 0x21696 reads the function at 0x1668 and passes
float gain in r0, lines in r1 and a 24-byte result pointer in r2 (base AAPCS).
The fill call at 0x22c2a reads 0x166c: four register arguments plus luma, HDR
flags and a pointer-based I2C result record on the stack. Compile-time checks
pin these records and all major descriptor boundaries. Unknown optional
test-pattern/extension records are deliberately zero and not advertised.

`test.sh` compiles and executes the new callbacks on ARM32 under qemu-user. It
checks invalid/non-finite gains, gain quantization and ISP residual gain, line
and frame bounds, group hold/release, buffer canaries, HDR rejection, sensor
program geometry and power sequencing.

`test-consumer.sh` checks the fixture hashes, builds a test-only ARM executable
and loads the actual new shared object. It relocates the two official fixture
ELFs in memory without their constructors. It executes the original angler
capability function first on the official IMX377 descriptor (control), then on
the candidate. Original angler conversion functions also verify its register
records, power-up description and CSID records against the GPL kernel types.
The actual fourcc conversion proves Bayer enum 2 produces GRBG10 (`BA10`).
The test caught and removed an unsupported I2C-mux power step; ordinary clock,
GPIO and regulator steps pass the original translation.

The fixture supplies only memory/string operations, zero log settings and a
logging sink. Other imported functions terminate the test. No camera service,
ioctl, property change, phone access or hardware programming is performed.
This is an interoperability test for the exercised interface, not proof of
every optional extension or a complete hardware session.

```sh
./test.sh KERNEL_REPO ARM_GCC_PREFIX PRIVATE_TEST_OUTPUT
./test-consumer.sh KERNEL_REPO ARM_GCC_PREFIX ANGLER_VENDOR_LIB PRIVATE_CONSUMER_OUTPUT
# Optional fifth argument checks an Android-built .so instead of the test build.
python3 generate-registers.py KERNEL_REPO --check
```

All fixture binaries, executables, generated logs and outputs stay outside Git.

## Sensor behavior

The first profile is DCC mode 4: 2496x1872 uncompressed RAW10, four CSI lanes,
assignment 0x0423, MCLK 9.6 MHz, line length 6024, frame length 2550, VT clock
460.8 MHz (29.99766 fps). The module advertises exactly this mode and matching
CSI/stream/crop counts. It uses the sensor's initial 8-bit address 0x20 and
checks silicon ID 0x0230 at 0x0016. No 0x34-to-0x20 software remap is used.

Programming is software reset, SMIA supply/EXTCLK prologue, the idle table,
mode table, frame length and initial unity gains. Zero table terminators are
removed rather than written to the ID register. Address register 0x0107 is
not programmed; the sensor stays at 0x20 rather than changing the address
behind the daemon. Whether this module needs the reference's address-change
sequence is a hardware validation question, not an emulation result.

Start and stop write MODE_SELECT 1 and 0. Exposure writes use group hold, frame
length, coarse integration and analogue gain. Gain follows the SMIA equation
512/(512-code), code 0..448. Unquantized excess gain is returned to the ISP;
sensor digital gains remain unity. There is no global keep-stream override,
ISP44 register rewrite or module-global exposure state.

The module never enables or disables shared LVS1/VIO; before a phone test,
verify that the existing touch consumer keeps that rail enabled. Other camera
rails and reset/MCLK use ordinary balanced kernel power steps.

## Deliberate limits and deployment

- No phone preview, saved image, power-cycle or sustained streaming is verified.
- Existing angler IMX377 chromatix names are an ABI-compatible ISP seed, **not
  Lumia/IMX230 tuning**. Optical metadata and black level are provisional;
  colour, 3A quality and optical/exposure limits need Cityman calibration.
- No front, AF/OIS, flash, HDR, PDAF, test patterns, embedded-data stream or
  full-resolution mode is advertised. These require their own implementation.
- The descriptor's disabled extension slots are not claimed to implement the
  whole proprietary SDK. Other daemon versions must not be assumed compatible.
- Product installation is gated by `CITYMAN_CAMERA_SMIA_EXPERIMENTAL=true`.
  The module is always explicitly buildable as `libmmcamera_imx230`.
- Hardware testing needs a kernel/vendor/HAL package from the Android 13 camera
  branches, fresh backup/hashes and coordination with the owner's reboot work.
  Do not inject this module into the owner's active audio/network tests.

## Credits and source provenance

- hraj9258, public SMIA++ register programs and clock/exposure findings,
  https://github.com/hraj9258/android_kernel_mmo_msm8994,
  `talkman-cam-clean` at `6ab0e0e9018d6185b642fa86de8b1c61cbd2701b`.
  Source `smia65pp_modes.h` SHA-256
  `744b2e10d70d0da125488df2d9cfe5a503bfaafb79deaffde246c81f479ae317`.
  `generate-registers.py` reproduces the converted arrays and authored default
  frame/exposure/unity-gain additions. No private Cityman DCC file is included.
- archienz, Talkman lane/clock/CCI identification and measurements,
  https://github.com/archienz/android_device_msft_talkman, `e33ff2c731ea`.
- Google angler factory image OPM7.181205.001, original plugin/consumer used
  only as private interoperability fixtures; camera driver `309d642245e2`.
- Android4Lumia950 (djokcho, EpicLPer, j0sh1x and community), Lumia base;
  LineageOS and LineageOS-UL, HAL/build/framework integration.
