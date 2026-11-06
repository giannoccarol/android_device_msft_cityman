# Iris IR camera (Cityman, ACPI `CAMT`)

Experimental test-stream module for the Lumia 950 XL iris scanner camera.
It is **off by default** and has **not been run on hardware**.

## Identification (offline)

- ACPI `CAMT` (`QCOM2436`), driver `qccamsensor_tertiarySMIApp8994.sys`
  (generic Nokia/Qualcomm SMIA++ wrapper, the same code as the rear and
  front drivers).
- CDCC `0C086101.dcc`: SMIA manufacturer `0x0C` (Toshiba), model `0x0861`,
  revision `0x01`; module "Jopo", Foxconn (`Jopo_Foxconn mp1.0` tuning).
  TechInsights lists the tertiary camera of this phone as a Toshiba CK71.
- One mode: 1920x1088 RAW10 out of a 1928x1088 array, CSI lane mode 0
  (one lane), 700.8 Mbps, 70.08 MHz pixel rate, line 2100, 1116 lines,
  29.9 fps. Gain code 56..448 (code/56). Production exposure 350 us.
- CSIPHY2 in 2+1 combo mode (PHY lanes 3/4) to CSID3, MCLK1 9.6 MHz,
  reset GPIO 102, standby GPIO 106, L3 1.2 V / L17 2.8 V / LVS1 1.8 V.
- Illuminator: PMI8994 flash block `0xd300` (ACPI `FLHT`), i.e. the
  `led:torch_0/1` / `led:flash_0/1` channels; the channel is not known.

Unknown until measured: CCI master and slave address (master 0 / 0x20 is
the guess), the value at `0x0000`, lane assignment (0x0004 or 0x0003),
settle count, black level, orientation.

## Private input

```
0C086101.dcc  sha256 656c1dd9c1d5a818a232319ef16b8ea7fa1936ce9ea40ed9c241ddbcaee86a85
```

`generate-ck71ir.py` checks the hash and emits `ck71ir_registers.generated.h`
(87 init writes from block `0x0000`, 30 mode writes from block `0xff00`).
The tables are written as the Windows driver does, including the non-standard
`0x0130/0x0131 = 0x099A` (9.6 MHz in 8.8 format).

## Build (opt-in)

```sh
export CITYMAN_CAMERA_IRIS=true
export CITYMAN_CAMERA_IRIS_DCC=/absolute/path/to/0C086101.dcc
m libmmcamera_ck71ir mm-qcamera-app
```

The daemon probes only the names compiled into
`libmmcamera2_sensor_modules.so`. `patch-sensor-list-iris.py` turns the
list tail `ov4688, vs6955` into `vs6955, ck71ir` so the iris is probed after
the front camera and HAL ids 0/1 do not move. Output stays private.

The HAL (`mm_camera_interface.c`) sorts the `ck71ir` camera last and hides it
unless `persist.vendor.camera.cityman.iris=1`.

`iris-ir-test.sh` lights one PMI8994 torch channel for a bounded time
(max 100 mA, 20 s) to find the IR LED.

## Credits

- Microsoft/Lumia stock driver pack (LumiaWoA `Lumia-Drivers`): the DCC,
  SMIA++ wrapper and ACPI wiring reference.
- The angler SMIA++ sensor ABI and the Cityman camera device tree are
  inherited from Android4Lumia950, LineageOS/LineageOS-UL, archienz and
  hraj9258 as recorded in the parent camera reports.
