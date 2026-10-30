/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.radio@1.4-service.legacy"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <hidl/HidlTransportSupport.h>

#include "Radio.h"
#include "hidl-utils.h"

#include <chrono>
#include <map>
#include <thread>

using namespace android::hardware::radio;
using namespace android::hardware::hidl_utils;

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::radio::implementation::Radio;

using android::OK;
using android::sp;
using android::status_t;

int slotId = 1;

// This service registers under the same instance name as rild, so the
// IRadio 1.0/1.1 entries of rild are replaced by ours. If this service dies
// while rild keeps running, those entries disappear with it and rild can no
// longer be found: if rild is running but unregistered after 30 s, restart it
// once so that it registers again. When rild itself dies, this service aborts
// through linkDeathToDeath and finds the new rild on restart, so the two
// services cannot restart each other in a loop.
static sp<V1_0::IRadio> getRildRadio(const std::string& name) {
    bool restarted = false;
    for (int i = 0; i < 120; ++i) {
        sp<V1_0::IRadio> radio = V1_0::IRadio::tryGetService(name);
        if (radio != nullptr) return radio;
        if (!restarted && i >= 30 &&
            android::base::GetProperty("init.svc.vendor.ril-daemon", "") == "running") {
            LOG(WARNING) << "rild is running but its IRadio " << name
                         << " is not registered, restarting rild";
            android::base::SetProperty("ctl.restart", "vendor.ril-daemon");
            restarted = true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return nullptr;
}

int main(int argc, char **argv) {
    // Note: Starts from slot 1
    std::map<int, sp<V1_4::IRadio>> slotIdToRadio;

    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "-s")) {
            slotId = 2;
            break;
        }
    }

    sp<V1_0::IRadio> realRadio = getRildRadio("slot" + std::to_string(slotId));
    if (realRadio == nullptr) {
        LOG(INFO) << "Cannot get radio service for slot " << slotId;
        return 1;
    }

    slotIdToRadio[slotId] = new Radio(realRadio);
    linkDeathToDeath(realRadio);

    configureRpcThreadpool(4, true);

    for (auto const& [slotId, radio] : slotIdToRadio) {
        status_t status = radio->registerAsService("slot" + std::to_string(slotId));
        if (status != OK) {
            LOG(ERROR) << "Cannot register Radio HAL service for slot " << slotId;
            return 1;
        }
    }

    LOG(INFO) << "Radio HAL service ready.";

    joinRpcThreadpool();

    LOG(INFO) << "Radio HAL service failed to join thread pool.";
    return 1;
}
