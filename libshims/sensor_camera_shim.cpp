/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// The angler camera stack (liboemcamera.so) calls the Android 8
// SensorManager::createEventQueue(String8, int). Android 12 added an
// attribution tag argument. Provide only the old symbol and forward it to
// the current method of the same system SensorManager object, so no other
// libsensor symbol is interposed.

#include <sensor/SensorEventQueue.h>
#include <sensor/SensorManager.h>
#include <utils/String16.h>
#include <utils/String8.h>

using android::sp;
using android::SensorEventQueue;
using android::SensorManager;
using android::String16;
using android::String8;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" sp<SensorEventQueue> _ZN7android13SensorManager16createEventQueueENS_7String8Ei(
        SensorManager* thiz, String8 packageName, int mode) {
    return thiz->createEventQueue(packageName, mode, String16(""));
}
#pragma clang diagnostic pop
