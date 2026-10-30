# Radio HAL wrappers for Android 16

The Android 16 telephony stack only talks to IRadio HIDL 1.4 or newer (or AIDL).
The open-source `libril` used by `rild` here implements IRadio 1.1, and the AOSP
AIDL compat service needs 1.5.

- `radio/`: `android.hardware.radio@1.4-service.cityman` gets rild's IRadio
  `slot1` and re-registers it as IRadio 1.4, converting requests, responses and
  indications between 1.0/1.1 and 1.2-1.4.
- `config/`: `android.hardware.radio.config@1.1-service.cityman`, a minimal
  IRadioConfig 1.1 (SIM slot status from the `vendor.radio.cardstate.slotN` and
  `vendor.radio.iccid.slotN` properties the wrapper sets). Reports one slot unless
  `persist.radio.multisim.config` is `dsds`/`dsda`.

## Credits

Taken from the OnePlus 2 Android 16 device tree,
[davigamer987/device_oneplus_oneplus2](https://github.com/davigamer987/device_oneplus_oneplus2)
(`lineage-23.2`, `hidl/radio` and `hidl/radio_config` at `a6bc99a`):

- Yaroslav Zviezda (acroreiser): the LineageOS radio 1.4 wrapper for msm8916 and
  msm8994 (legacy dual SIM, manual network selection, emergency dial) and the
  dummy RadioConfig 1.1;
- Davi (davigamer987): wrapper integration, slot registration, getSimSlotsStatus.

Cityman changes: module names, a single `slot1` service, and the slot count in
`getSimSlotsStatus`.
