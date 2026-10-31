# Cityman front camera exposure tuning

The front VS6955 loads the angler (Nexus 6P) front camera's `imx179_liteon`
chromatix from the vendor tree. Three of them are rebuilt here from C:

| Library | Donor | Used for |
|---|---|---|
| `libchromatix_vs6955_preview.so` | `libchromatix_imx179_liteon_preview.so` | preview and snapshot |
| `libchromatix_vs6955_video_binning.so` | `libchromatix_imx179_liteon_video_binning.so` | video |
| `libchromatix_vs6955_liveshot.so` | `libchromatix_imx179_liteon_liveshot.so` | liveshot |

`camera-sensors/vs6955/sensor_lib_vs6955.c` names them; the common, CPP and
postproc chromatix remain the vendor's imx179_liteon blobs.

## What changes

Only the auto-exposure luma targets, three bytes per library:

| Target | Donor | Here |
|---|---|---|
| default (indoor), struct `0xc07c` | 51 | 62 |
| outdoor, struct `0xc07d` | 50 | 58 |
| lowlight, struct `0xe310` | 45 | 50 |

Indoors the front preview converged on target 51 at ISO 1889, 1/15 s and still
looked dark. The lowlight target rises less because the gain is already high
there. The fields and the evidence for their offsets are documented in the
rear generator (`../rear-tuning/chromatix/gen-chromatix-c.py`, `FIELDS`).

## Regenerating

```
CHX_DONOR=<vendor/msft/cityman/proprietary/vendor/lib> ./gen-front-chromatix.py verify
CHX_DONOR=<...> ./gen-front-chromatix.py emit
```

`verify` checks that the emitted payloads equal the donor except for the
overrides (3 bytes per library).

## Credits

- **hraj9258** (Himanshu Raj), talkman port
  ([gitlab.com/hraj9258/android_device_msft_talkman](https://gitlab.com/hraj9258/android_device_msft_talkman)):
  the chromatix container rebuilt in C and the generator this reuses.
- **Google / Qualcomm**: the angler imx179_liteon tuning used as donor.
