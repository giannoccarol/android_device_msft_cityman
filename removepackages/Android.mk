LOCAL_PATH := $(call my-dir)

# Apps of the LineageOS product that only cost RAM, storage or background
# work on this device: the OTA updater (no server for unofficial builds) and
# the old gallery (Glimpse is installed).
include $(CLEAR_VARS)
LOCAL_MODULE := RemovePackages
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_OVERRIDES_PACKAGES := Updater Gallery2
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_SRC_FILES := /dev/null
include $(BUILD_PREBUILT)
