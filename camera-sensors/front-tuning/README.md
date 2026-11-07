# Cityman front camera tuning

The front VS6955 loads the angler (Nexus 6P) front camera's `imx179_liteon`
chromatix from the vendor tree. The three VFE/3A payloads, and optionally the
common one, are rebuilt here from C:

| Library | Donor | Used for |
|---|---|---|
| `libchromatix_vs6955_<set>_preview.so` | `libchromatix_imx179_liteon_preview.so` | preview and snapshot |
| `libchromatix_vs6955_<set>_video_binning.so` | `libchromatix_imx179_liteon_video_binning.so` | video |
| `libchromatix_vs6955_<set>_liveshot.so` | `libchromatix_imx179_liteon_liveshot.so` | liveshot |
| `libchromatix_vs6955_common.so` (only with `CITYMAN_CAMERA_FRONT_BLACK`) | `libchromatix_imx179_liteon_common.so` | linearization (black level) |

`<set>` is the colour matrix: `imx179` (the donor's), `swarc` or `hwqc` (one
matrix of the module's Windows calibration `0A214000.dcc` in all eight
entries). `camera-sensors/vs6955/sensor_lib_vs6955.c` names them and picks
the set; the CPP and postproc chromatix remain the vendor's imx179_liteon
blobs. The options, how to switch them on the phone and the tests are in
`../vs6955/README.md` ("Colour options").

## What changes

The auto-exposure luma targets, three bytes per VFE library:

| Target | Donor | Here |
|---|---|---|
| default (indoor), struct `0xc07c` | 51 | 62 |
| outdoor, struct `0xc07d` | 50 | 58 |
| lowlight, struct `0xe310` | 45 | 50 |

Indoors the front preview converged on target 51 at ISO 1889, 1/15 s and still
looked dark. The lowlight target rises less because the gain is already high
there. The fields and the evidence for their offsets are documented in the
rear generator (`../rear-tuning/chromatix/gen-chromatix-c.py`, `FIELDS`).

In the `swarc` and `hwqc` sets, the colour matrices (`ccm_0`..`ccm_7`, struct
`0x40a8`, stride `0x2c`). The `.inc` files keep the donor's matrices inside
`CITYMAN_FRONT_CCM(i, ...)`; `chromatix_front.c` replaces them with the
matrix from a header that `gen-front-ccm.py` writes into the build output
from the hash-checked DCC. The DCC and that header must never be committed.

In `libchromatix_vs6955_common`, the start of every linearization knee
ladder (24 ladders from struct `0x0864`), shifted to the given black level
with the donor's widths kept: `CITYMAN_FRONT_LIN(ch, start, v)` in
`chx_front_common.inc`, range-checked at compile time.

## Regenerating

```
CHX_DONOR=<vendor/msft/cityman/proprietary/vendor/lib> ./gen-front-chromatix.py verify
CHX_DONOR=<...> ./gen-front-chromatix.py emit
```

`verify` checks that the emitted payloads equal the donor except for the
overrides (3 bytes per VFE library, none in common). After a build,

```
CHX_DONOR=<...> ./gen-front-chromatix.py verify-built <dir of the built .so> <0A214000.dcc>
```

checks the libraries themselves: everything but the declared fields equals
the donor, all eight matrices are the same DCC matrix (it names which, without
printing it), and the black level ladders keep their widths.

`./gen-front-ccm.py <dcc> list` prints the twelve DCC matrices and their
means to the terminal, for analysis only.

## Credits

- **hraj9258** (Himanshu Raj), talkman port
  ([gitlab.com/hraj9258/android_device_msft_talkman](https://gitlab.com/hraj9258/android_device_msft_talkman)):
  the chromatix container rebuilt in C and the generator this reuses.
- **Google / Qualcomm**: the angler imx179_liteon tuning used as donor.
- **Microsoft / Lumia**: the VS6955 colour matrices of the stock calibration
  `0A214000.dcc` (FFU 10586.13169), read at build time and not redistributed.
