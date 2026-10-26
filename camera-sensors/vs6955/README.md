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

- Bayer/CFA order (the current value is a valid placeholder, not a measurement).
- CSI lane assignment (`0x3210` provisional) and settle count (27 provisional).
- Frame length (1125), VT/output clock (92.8125 MHz) and FPS (30) are derived
  from the line length and a 30 fps assumption; they must be measured.
- SMIA++ exposure/gain mapping and the firmware patch timing.
- Per-unit OTP/calibration and the front DCC colour blocks.
- There is no autofocus and no flash for this sensor.

## Credits

- Microsoft/Lumia stock FFU build 10586.13169 and the LumiaWoA driver pack:
  the front DCC, SMIA++ driver and ACPI wiring reference.
- The reverse-engineering work and extracted tables are kept in the project
  private evidence directory; no vendor table is committed here.
- The angler SMIA++ sensor ABI and the Cityman device tree are inherited from
  Android4Lumia950, LineageOS/LineageOS-UL, archienz and hraj9258 as recorded
  in the parent camera reports.
