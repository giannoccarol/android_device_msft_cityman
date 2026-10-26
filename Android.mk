#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This contains the module build definitions for the hardware-specific
# components for this device.
#
# As much as possible, those components should be built unconditionally,
# with device-specific names to avoid collisions, to avoid device-specific
# bitrot and build breakages. Building a component unconditionally does
# *not* include it on all devices, so it is safe even with hardware-specific
# components.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := cityman_wlan_mac
include $(BUILD_PHONY_PACKAGE)

# qcacld reads the WLAN MAC from wlan/qca_cld/wlan_mac.bin: the Lumia
# platform data has none, and without the file the driver resets the chip
# in a loop. Link it to the per-device file in /persist. Android 13 no
# longer runs LOCAL_POST_INSTALL_CMD for phony packages, so use a rule.
WLAN_MAC_SYMLINK := $(TARGET_OUT_ETC)/firmware/wlan/qca_cld/wlan_mac.bin
$(WLAN_MAC_SYMLINK):
	@mkdir -p $(dir $@)
	@ln -sf /persist/wlan_mac.bin $@
ALL_DEFAULT_INSTALLED_MODULES += $(WLAN_MAC_SYMLINK)

ifneq ($(filter cityman, $(TARGET_DEVICE)),)

include $(call all-makefiles-under,$(LOCAL_PATH))

# GPS HAL: BOARD_USES_QCOM_HARDWARE hides hardware/qcom/gps and there is no
# qcom-caf GPS tree for msm8994, so build the msm8994 one from here.
include $(call all-makefiles-under,hardware/qcom/gps/msm8994)

# Audio HAL: the generic hardware/qcom/audio, the one the 18.1 ROM and its
# speaker/call audio run with (BOARD_USES_QCOM_HARDWARE hides it; the
# qcom-caf msm8994 audio project is left out of the manifest).
include hardware/qcom/audio/Android.mk

DM_LIBS := libdmengine.so libdmjavaplugin.so
DM_SYMLINKS := $(addprefix $(TARGET_OUT)/priv-app/DMService/lib/arm/,$(notdir $(DM_LIBS)))
$(DM_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "DMService lib link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /system/lib/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(DM_SYMLINKS)

IMS_LIBS := libimscamera_jni.so libimsmedia_jni.so
IMS_SYMLINKS := $(addprefix $(TARGET_OUT_VENDOR)/app/ims/lib/arm64/,$(notdir $(IMS_LIBS)))
$(IMS_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "IMS lib link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /vendor/lib64/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(IMS_SYMLINKS)

endif
