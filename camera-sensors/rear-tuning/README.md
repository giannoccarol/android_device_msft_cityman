# Cityman rear camera tuning

Libraries for the rear IMX230 module, built from C:

| Library | Source | What it is |
|---|---|---|
| `libchromatix_imx230_*.so` (10) | `chromatix/chromatix_lib.c` + `chromatix/data/*.inc` | ISP and 3A tuning |
| `libactuator_cityman.so` | `actuator_lib_cityman.c` | lens actuator driver data |
| `libactuator_cityman_{camera,camcorder}.so` | `af/af_tuning_lib.c` + `af/data/*.inc` | AF search tuning |

`camera-sensors/imx230/sensor_lib_imx230.c` names these chromatix libraries and,
with `CITYMAN_CAMERA_AF_NAME=cityman` (the default), this actuator.

## Origin

All of it comes from hraj9258's talkman (Lumia 950) port: Himanshu Raj,
[gitlab.com/hraj9258/android_device_msft_talkman](https://gitlab.com/hraj9258/android_device_msft_talkman),
branch `lineage-18.1-talkman`, `camera/sensors/imx230` at `901b039`. The 950 and the
950 XL share the rear module, its lens controller and the Windows calibration file
`0AEACA05.dcc`. His work that matters most here:

- the chromatix containers rebuilt in C from the angler IMX377 donor, with the
  module's measured per-channel black level (R 256, Gr 253, Gb 256, B 255 at
  12 bits instead of the donor's 210) and the module's own Daylight colour matrix
  from `0AEACA05.dcc`. The donor's black level left 46 counts in every pixel,
  which white balance and the matrix turned into the magenta cast;
- the AF search tuning in C (donor `lc898212xd`) with the step fields rescaled to
  this lens;
- the actuator: the lens controller at 0x7C takes the high byte of the position
  first (`ACTUATOR_VCM_BE`, kernel side in `msm_actuator.c`), and `qvalue` never
  reaches the kernel, so the step size is a whole number of codes.

`chromatix/gen-chromatix-c.py verify` and `af/gen-af-c.py verify` report every
payload identical to the donor apart from the declared overrides. The donor must be
the angler IMX377 as shipped: the cityman vendor tree replaced its `common` and
`cpp_*` libraries in `d859261` with an Android 13 tuning, so point the chromatix
generator at the earlier files:

```
CHX_DONOR=<dir with the libchromatix_imx377_*.so of d859261^> ./gen-chromatix-c.py verify
```

`data/chx_common.inc` is the talkman file as shipped. Its lens rolloff mesh
(struct offsets 0x8bc8..0xa768) differs from the angler donor, and the rear colours
were confirmed with it, so regenerate selectively (`emit cpp_`) rather than
overwriting it.

## Cityman changes

- Library and symbol names `talkman` → `cityman`.
- Lens positions from this phone's module NVM (`dumps/af-diag-20261027` in the
  project folder), read as running sums like the talkman ones: far end 626,
  infinity 909, mid 1178, near end 1452 (talkman: 654 / 900 / 1149 / 1437).
  The step table runs 909..1451 in 271 steps of 2 codes. The older
  `camera-sensors/carrera-af` used 909..1178, the mid point, so only half the
  travel, with a `qvalue` that the daemon replaces.
- The AF tuning's step fields stay as tuned for the talkman's 268 steps (1% apart).
- The OIS init sequence of `carrera-af` is not used, as on the talkman.
- Stronger CPP wavelet denoise (31/10): the noise profiles of all six gain
  entries scaled by 1.4 for luma and 1.6 for Cb/Cr, declared as overrides in
  `gen-chromatix-c.py`. It is the scaling tested on this phone with the Android 13
  camera (vendor `d859261`), and the built `cpp_*` payloads are bit-identical to
  those Android 13 libraries. Reduces the colour grain of the rear camera.

Not yet verified on a cityman: these libraries were built and checked against
the donors, the phone test is pending.
