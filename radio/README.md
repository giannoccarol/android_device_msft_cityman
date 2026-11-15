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

Mobile data on either SIM: RadioConfig's `setPreferredDataModem` writes the
chosen logical modem to `vendor.radio.cityman.data_slot`. The wrapper of each
slot watches it and sends its rild `setDataAllowed` (true for the chosen slot,
false for the other), which qcril handles with a DDS switch
(`RIL_REQUEST_ALLOW_DATA`); those requests use serials from 0x7eda0000 and
their responses are not forwarded to telephony. The wrapper of the chosen slot
writes `vendor.radio.cityman.data_ack`, and RadioConfig waits up to 5 s for it
before answering.

Calls: in DSDS mode qcril leaves its "init ongoing" state, and only then
handles voice indications, after `RIL_REQUEST_SET_UICC_SUBSCRIPTION`, which
Android 16 never sends. Without it outgoing calls stayed in DIALING and
incoming calls never rang. The wrapper of each slot sends `setUiccSubscription`
once per telephony connection when rild reports the card present.

Call audio on SIM 2: the wrapper writes the slot of the call in progress to
`vendor.radio.cityman.voice_slot` (on dial, answer and from the call list; 0
when no call is left). The audio HAL patch
`hardware/qcom-caf/msm8994/audio/0001` of the manifest starts the second voice
session (`voice2-call`, VSID 0x10DC1000) when it reads 2. The value is the
modem stack of the slot's rild plus one, read from
`persist.radio.msim.stackid_N`: slot 1 and 2 normally, swapped after Android
moves the LTE capability to SIM 2 (`setRadioCapability`, done when the data
SIM changes), when the second voice session belongs to SIM 1. After such a
change the calls are reliable only after a reboot: at runtime qcril rebinds
the rild instances without always moving the cards between the sessions.

## Credits

Taken from the OnePlus 2 Android 16 device tree,
[davigamer987/device_oneplus_oneplus2](https://github.com/davigamer987/device_oneplus_oneplus2)
(`lineage-23.2`, `hidl/radio` and `hidl/radio_config` at `a6bc99a`):

- Yaroslav Zviezda (acroreiser): the LineageOS radio 1.4 wrapper for msm8916 and
  msm8994 (legacy dual SIM, manual network selection, emergency dial) and the
  dummy RadioConfig 1.1;
- Davi (davigamer987): wrapper integration, slot registration, getSimSlotsStatus.

Cityman changes: module names, the `slot2` wrapper only on dual SIM models
(and its rild name in the restart check), the slot and modem counts in
RadioConfig, and the data slot switch.
