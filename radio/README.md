# Radio HAL wrappers for Android 16

The Android 16 telephony stack only talks to IRadio HIDL 1.4 or newer (or AIDL).
The open-source `libril` used by `rild` here implements IRadio 1.1, and the AOSP
AIDL compat service needs 1.5.

- `radio/`: `android.hardware.radio@1.4-service.cityman` gets rild's IRadio
  `slot1` and re-registers it as IRadio 1.4, converting requests, responses and
  indications between 1.0/1.1 and 1.2-1.4.
- `config/`: `android.hardware.radio.config@1.1-service.cityman`, a minimal
  IRadioConfig 1.1 (SIM slot status from the `vendor.radio.cardstate.slotN` and
  `vendor.radio.iccid.slotN` properties the wrapper sets). Reports one slot and
  one logical modem unless `persist.radio.multisim.config` is `dsds`/`dsda`.

## Dual SIM (RM-1116, RM-1118)

`init.cityman.simconfig.sh` reads the model from the DPP partition at
post-fs-data and sets `persist.radio.multisim.config=dsds` on the dual SIM
models. That property enables `vendor.ril-daemon2` (`rild -c 2`) and a second
wrapper, `vendor.radio-1-4-cityman2` (`-s`, IRadio `slot2`); the manifest
declares `slot2`, RadioConfig reports two slots and two logical modems, and the
UIM bootstrap (kernel `tools/cityman-ril`) opens the secondary GW session for
the card in the second slot. Single SIM models are unchanged.

Not done yet: `setPreferredDataModem` still only acknowledges, so mobile data
stays on the subscription the modem uses by default (expected: the first SIM).

## Credits

Taken from the OnePlus 2 Android 16 device tree,
[davigamer987/device_oneplus_oneplus2](https://github.com/davigamer987/device_oneplus_oneplus2)
(`lineage-23.2`, `hidl/radio` and `hidl/radio_config` at `a6bc99a`):

- Yaroslav Zviezda (acroreiser): the LineageOS radio 1.4 wrapper for msm8916 and
  msm8994 (legacy dual SIM, manual network selection, emergency dial) and the
  dummy RadioConfig 1.1;
- Davi (davigamer987): wrapper integration, slot registration, getSimSlotsStatus.

Cityman changes: module names, the `slot2` wrapper only on dual SIM models
(and its rild name in the restart check), and the slot and modem counts in
RadioConfig.
