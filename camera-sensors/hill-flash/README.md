# Rear flash: libflash_hill

The Lumia 950 XL rear LEDs are driven by the same "Hill" three-channel flash
IC as the Lumia 950: 8-bit address 0x60 on CCI master 0, ID register 0x00 =
0x16. They are not on PMI8994: in the cityman DSDT the rear flash (`FLSH`,
`QCOM244B`) has no resource, and PMI8994's flash block (peripheral 0xd300,
interrupts 0xe9e/0xe9f) belongs to `FLHT`, the tertiary flash that follows
the iris camera (`CAMT`). See the kernel's
`arch/arm64/boot/dts/mmo/common/camera-cityman.dtsi`.

## How the flash is reached

- **Camera** (preflash, flash, video torch): the angler daemon reads
  `flash_name = "hill"` from `libmmcamera_imx230`, finds the kernel flash
  subdev through `qcom,led-flash-src` on `qcom,camera@0`, and dlopens
  `libflash_hill.so`. `flash_driver_open_lib()` returns the struct with the
  slave address, the GPIO 12 power sequence and the register arrays for
  `CFG_FLASH_INIT/OFF/LOW/HIGH`; the kernel (msm_flash.c, FLASH_DRIVER_I2C)
  writes them as they are.
- **Flashlight tile**: `QCameraFlash` in the HAL opens the same subdev and
  sends `CFG_FLASH_INIT` and `CFG_FLASH_LOW` without register settings; the
  kernel falls back to `qcom,flash-low-setting` in the device tree (torch
  level 4). No userspace library is involved.
- **Kernel only**: `/sys/class/leds/flash-torch` (hill.c), brightness
  1..255 maps to levels 1..7.

Operating points, all from hraj9258's measurements on the 950: preflash and
video torch level 2 (the ratio to level 7 matches the chromatix
`aec_flash_flux_ratio` 7.0), main flash level 7 in torch mode, switched off
by the kernel after `flash_high_timeout_ms` (1500 ms) because the daemon
sends no `CFG_FLASH_OFF` after `CFG_FLASH_HIGH`. The chromatix LED white
point is already Hill's (`rear-tuning/chromatix/gen-chromatix-c.py`).

## Build switch

`CITYMAN_CAMERA_FLASH` (default `true`, inside
`CITYMAN_CAMERA_SMIA_EXPERIMENTAL`) installs `libflash_hill` and makes the
sensor library declare the flash. `false` restores the previous behaviour
(no flash). Either way works with a kernel without the Hill node: the
kernel then reports no flash subdev for the sensor and the daemon does not
load the library.

## Not verified on the cityman

- that the part at 0x60 on CCI master 0 answers 0x16 (the kernel checks and
  logs `msm_flash: id 0x16 at 8-bit 0x60 on cci master 0`, otherwise it
  refuses to write);
- the levels, the white point and the flux ratio, measured on the 950 and
  assumed equal (same module family, same flash driver package).

## Credits

`flash_lib_hill.c` is by hraj9258 (Himanshu Raj),
`camera/sensors/imx230/flash_lib_hill.c` in
gitlab.com/hraj9258/android_device_msft_talkman, branch
`lineage-18.1-talkman`, copied unchanged: struct layout recovered from the
angler `libmmcamera2_sensor_modules.so`, Hill's register map from the
Lumia 950's `qccamflash8992.sys`, operating points measured on the phone.
The kernel side is his commit bb9466c on
github.com/hraj9258/android_kernel_mmo_msm8994 (`talkman-cam-clean`).
