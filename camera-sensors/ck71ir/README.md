# Iris IR camera (Cityman, ACPI `CAMT`)

Sensor module for the Lumia 950 XL iris scanner camera, used by the iris unlock
face HAL (giannoccarol/cityman-iris). Built only with `CITYMAN_CAMERA_IRIS=true`.
It streams on hardware since 6/11/2026 (CSID3, 0 ECC/CRC errors) with a kernel
that has the cityman iris CSIPHY changes (MISC1 bit 1 on PHY2 lanes 3/4).

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
- Illuminator: PMI8994 flash block `0xd300` (ACPI `FLHT`). Both
  `led:torch_0` and `led:torch_1` feed it and their currents add.

Measured on the phone: CCI master 0, 8-bit slave **0x6C**, model `0x0861`;
CSID3 `lane_assign 0x0004`, settle 22, PHY2 `lane_mask 0x18` in combo mode;
black level 64; monochrome (the four CFA sites match within 3 %, the BGGR
order is only a label). `sensor_lib_ck71ir.c` reads `vendor.camera.iris.*`
overrides for bring-up (lane_assign, settle, lane_mask, combo, standby, reg).

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
m libmmcamera_ck71ir
```

The daemon probes only the names compiled into
`libmmcamera2_sensor_modules.so`. `patch-sensor-list-iris.py` turns the
list tail `ov4688, vs6955` into `vs6955, ck71ir` so the iris is probed after
the front camera and HAL ids 0/1 do not move (ov4688 is not fitted on cityman);
the vendor tree ships the patched list.

The HAL sorts the `ck71ir` camera last (id 2), forces the mono effect on it
(`ro.vendor.camera.mono_ids=2`) and marks it `SYSTEM_CAMERA`: apps see it only
with `persist.vendor.camera.cityman.iris=1`, the face HAL always.

`iris-ir-test.sh` lights one PMI8994 torch channel for a bounded time
(max 100 mA, 20 s). Kernels with `qcom,auto-off-ms` on the torch nodes switch
it off after 100 ms, so use pulsed tools (cityman-iris `iris_grab -S`).

## Credits

- Microsoft/Lumia stock driver pack (LumiaWoA `Lumia-Drivers`): the DCC,
  SMIA++ wrapper and ACPI wiring reference.
- The angler SMIA++ sensor ABI and the Cityman camera device tree are
  inherited from Android4Lumia950, LineageOS/LineageOS-UL, archienz and
  hraj9258 as recorded in the parent camera reports.
