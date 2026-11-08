/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "RadioConfig.h"
#include <android/hardware/radio/1.0/types.h>
#include <cutils/properties.h>

namespace android {
namespace hardware {
namespace radio {
namespace config {

using namespace ::android::hardware::radio::config::V1_1;
using ::android::hardware::radio::V1_0::RadioError;
using ::android::hardware::radio::V1_0::RadioResponseInfo;
using ::android::hardware::radio::V1_0::RadioResponseType;

// The framework matches a response to its request by serial: an
// uninitialised info was dropped, the request never completed, and
// PhoneSwitcher never published the active data subscription (the status
// bar then showed the mobile signal with an exclamation mark).
static RadioResponseInfo responseInfo(int32_t serial,
                                      RadioError error = RadioError::NONE) {
    RadioResponseInfo info = {};
    info.type = RadioResponseType::SOLICITED;
    info.serial = serial;
    info.error = error;
    return info;
}

Return<void> RadioConfig::setResponseFunctions(
    const sp<V1_0::IRadioConfigResponse>& radioConfigResponse,
    const sp<V1_0::IRadioConfigIndication>& radioConfigIndication) {
    mRadioConfigResponse = V1_1::IRadioConfigResponse::castFrom(radioConfigResponse);
    mRadioConfigIndication = radioConfigIndication;
    return Void();
}

// One SIM slot unless the device is configured for dual SIM.
static uint32_t simSlotCount() {
    char prop[PROPERTY_VALUE_MAX];
    property_get("persist.radio.multisim.config", prop, "");
    return (!strcmp(prop, "dsds") || !strcmp(prop, "dsda")) ? 2 : 1;
}

Return<void> RadioConfig::getSimSlotsStatus(int32_t serial) {
    const uint32_t slots = simSlotCount();
    hidl_vec<V1_0::SimSlotStatus> slotStatus(slots);
    for (uint32_t i = 0; i < slots; ++i) {
        char prop[PROPERTY_VALUE_MAX];
        char key[PROPERTY_KEY_MAX];

        snprintf(key, sizeof(key), "vendor.radio.cardstate.slot%d", i + 1);
        property_get(key, prop, "0" /* ABSENT */);
        int cardState = atoi(prop);

        slotStatus[i].cardState = static_cast<::android::hardware::radio::V1_0::CardState>(cardState);
        slotStatus[i].slotState = V1_0::SlotState::ACTIVE;
        slotStatus[i].logicalSlotId = i;
        slotStatus[i].atr = "";

        if (slotStatus[i].cardState == ::android::hardware::radio::V1_0::CardState::PRESENT) {
            snprintf(key, sizeof(key), "vendor.radio.iccid.slot%d", i + 1);
            property_get(key, prop, "");
            slotStatus[i].iccid = prop;
        }
    }

    mRadioConfigResponse->getSimSlotsStatusResponse(responseInfo(serial), slotStatus);
    return Void();
}

Return<void> RadioConfig::setSimSlotsMapping(int32_t serial,
                                             const hidl_vec<uint32_t>& /* slotMap */) {
    mRadioConfigResponse->setSimSlotsMappingResponse(responseInfo(serial));
    return Void();
}

// One modem, data on one logical modem at a time.
Return<void> RadioConfig::getPhoneCapability(int32_t serial) {
    V1_1::PhoneCapability capability = {};
    capability.maxActiveData = 1;
    capability.maxActiveInternetData = 1;
    capability.isInternetLingeringSupported = false;
    capability.logicalModemList.resize(1);
    capability.logicalModemList[0].modemId = 0;
    mRadioConfigResponse->getPhoneCapabilityResponse(responseInfo(serial), capability);
    return Void();
}

// rild has a single data stack: there is nothing to switch.
Return<void> RadioConfig::setPreferredDataModem(int32_t serial, uint8_t /* modemId */) {
    mRadioConfigResponse->setPreferredDataModemResponse(responseInfo(serial));
    return Void();
}

Return<void> RadioConfig::setModemsConfig(int32_t serial,
                                          const V1_1::ModemsConfig& /* modemsConfig */) {
    mRadioConfigResponse->setModemsConfigResponse(responseInfo(serial));
    return Void();
}

Return<void> RadioConfig::getModemsConfig(int32_t serial) {
    V1_1::ModemsConfig config = {};
    config.numOfLiveModems = 1;
    mRadioConfigResponse->getModemsConfigResponse(responseInfo(serial), config);
    return Void();
}

}  // namespace config
}  // namespace radio
}  // namespace hardware
}  // namespace android
