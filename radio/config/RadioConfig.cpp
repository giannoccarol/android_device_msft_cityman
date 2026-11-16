/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <thread>
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

// One logical modem per SIM slot (dual SIM dual standby), data on one at a time.
Return<void> RadioConfig::getPhoneCapability(int32_t serial) {
    const uint32_t slots = simSlotCount();
    V1_1::PhoneCapability capability = {};
    capability.maxActiveData = 1;
    capability.maxActiveInternetData = 1;
    capability.isInternetLingeringSupported = false;
    capability.logicalModemList.resize(slots);
    for (uint32_t i = 0; i < slots; ++i)
        capability.logicalModemList[i].modemId = i;
    mRadioConfigResponse->getPhoneCapabilityResponse(responseInfo(serial), capability);
    return Void();
}

// Dual SIM: move mobile data to MODEMID. Each rild instance is reached only
// through its IRadio wrapper, so the slot goes to the wrappers through
// vendor.radio.cityman.data_slot: they send setDataAllowed (qcril moves the
// DDS) and the wrapper of the chosen slot writes vendor.radio.cityman.data_ack
// once rild accepted it. Wait up to 5 s for that before answering, so that
// telephony sets up data on the new slot after the switch. Single SIM: there
// is nothing to switch.
//
// The data SIM should also have the multimode (LTE) stack 0. The radio wrappers
// refuse Android's runtime radio capability switch, which qcril does not carry
// out reliably; save the order instead in the properties qcril reads when it
// starts (persist.radio.msim.stackid_N, persist.radio.stack_id_N), so that the
// data SIM gets LTE after the next reboot. The bootstrap of the UIM sessions
// follows the same properties.
static void saveStackOrder(uint8_t dataSlot) {
    char key[PROPERTY_KEY_MAX];

    if (dataSlot > 1) return;
    for (int slot = 0; slot < 2; ++slot) {
        const char* stack = slot == dataSlot ? "0" : "1";
        snprintf(key, sizeof(key), "persist.radio.msim.stackid_%d", slot);
        property_set(key, stack);
        snprintf(key, sizeof(key), "persist.radio.stack_id_%d", slot);
        property_set(key, stack);
    }
}

static void setDataSlot(uint8_t modemId) {
    char want[8], ack[PROPERTY_VALUE_MAX];

    saveStackOrder(modemId);
    snprintf(want, sizeof(want), "%u", modemId);
    property_set("vendor.radio.cityman.data_slot", want);
    for (int i = 0; i < 50; ++i) {
        property_get("vendor.radio.cityman.data_ack", ack, "");
        if (!strcmp(ack, want)) break;
        usleep(100 * 1000);
    }
}

// A call is in progress when the radio wrappers have written its voice
// session to vendor.radio.cityman.voice_slot (dual SIM only). A value left
// there for over 3 hours is taken as stale, so that it cannot hold the data
// slot for ever.
static bool callInProgress() {
    static std::atomic<long> since{0};
    char session[PROPERTY_VALUE_MAX];
    struct timespec now;

    property_get("vendor.radio.cityman.voice_slot", session, "0");
    clock_gettime(CLOCK_BOOTTIME, &now);
    if (!session[0] || !strcmp(session, "0")) {
        since = 0;
        return false;
    }
    if (!since) since = now.tv_sec;  // a race here only shifts the start by a poll
    return now.tv_sec - since < 3 * 3600;
}

// On this DSDS modem (one radio, the second stack GSM only) Android moves
// data to the SIM in a call and back when the call ends. After that round
// trip the data SIM stayed on 2G/3G without data until airplane mode, and
// data is suspended during a call anyway: while a call is in progress, keep
// the data slot and remember the request; a request still pending a few
// seconds after the call (the user changed the data SIM meanwhile) is applied
// then.
static std::mutex gPendingLock;
static int gPendingModem = -1;
static bool gPendingWatcher = false;

static void watchPendingDataSlot() {
    for (;;) {
        while (callInProgress()) usleep(500 * 1000);
        sleep(3);
        std::unique_lock<std::mutex> lock(gPendingLock);
        if (callInProgress()) continue;
        const int modem = gPendingModem;
        gPendingModem = -1;
        gPendingWatcher = false;
        lock.unlock();
        if (modem >= 0) setDataSlot(modem);
        return;
    }
}

Return<void> RadioConfig::setPreferredDataModem(int32_t serial, uint8_t modemId) {
    if (simSlotCount() > 1) {
        std::unique_lock<std::mutex> lock(gPendingLock);
        if (callInProgress()) {
            gPendingModem = modemId;
            if (!gPendingWatcher) {
                gPendingWatcher = true;
                std::thread(watchPendingDataSlot).detach();
            }
        } else {
            gPendingModem = -1;
            lock.unlock();
            setDataSlot(modemId);
        }
    }
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
    config.numOfLiveModems = simSlotCount();
    mRadioConfigResponse->getModemsConfigResponse(responseInfo(serial), config);
    return Void();
}

}  // namespace config
}  // namespace radio
}  // namespace hardware
}  // namespace android
