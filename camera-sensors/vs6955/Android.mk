LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(CITYMAN_CAMERA_FRONT_VS6955),true)
LOCAL_MODULE := libmmcamera_vs6955
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := sensor_lib_vs6955.c
LOCAL_CFLAGS := -std=gnu11 -Wall -Wextra -Werror -fvisibility=hidden
LOCAL_CFLAGS += -DCITYMAN_CAMERA_FRONT_VS6955
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../imx230 $(TARGET_KERNEL_SOURCE)/include
ifeq ($(strip $(CITYMAN_CAMERA_FRONT_DCC)),)
$(error vs6955 requires CITYMAN_CAMERA_FRONT_DCC pointing to the pinned private 0A214000.dcc)
endif
cityman_front_header := $(call local-generated-sources-dir)/vs6955_registers.generated.h
LOCAL_GENERATED_SOURCES += $(cityman_front_header)
LOCAL_C_INCLUDES += $(dir $(cityman_front_header))
$(cityman_front_header): PRIVATE_DCC := $(CITYMAN_CAMERA_FRONT_DCC)
$(cityman_front_header): PRIVATE_GENERATOR := $(LOCAL_PATH)/generate-vs6955.py
$(cityman_front_header): $(CITYMAN_CAMERA_FRONT_DCC) $(LOCAL_PATH)/generate-vs6955.py
	python3 $(PRIVATE_GENERATOR) "$(PRIVATE_DCC)" "$@"
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $(BUILD_SHARED_LIBRARY)
endif
