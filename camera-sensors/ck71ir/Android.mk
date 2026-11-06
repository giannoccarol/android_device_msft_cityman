LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(CITYMAN_CAMERA_IRIS),true)
LOCAL_MODULE := libmmcamera_ck71ir
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := sensor_lib_ck71ir.c
LOCAL_CFLAGS := -std=gnu11 -Wall -Wextra -Werror -fvisibility=hidden
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../imx230 $(TARGET_KERNEL_SOURCE)/include
ifeq ($(strip $(CITYMAN_CAMERA_IRIS_DCC)),)
$(error ck71ir requires CITYMAN_CAMERA_IRIS_DCC pointing to the pinned private 0C086101.dcc)
endif
cityman_iris_header := $(call local-generated-sources-dir)/ck71ir_registers.generated.h
LOCAL_GENERATED_SOURCES += $(cityman_iris_header)
LOCAL_C_INCLUDES += $(dir $(cityman_iris_header))
$(cityman_iris_header): PRIVATE_DCC := $(CITYMAN_CAMERA_IRIS_DCC)
$(cityman_iris_header): PRIVATE_GENERATOR := $(LOCAL_PATH)/generate-ck71ir.py
$(cityman_iris_header): $(CITYMAN_CAMERA_IRIS_DCC) $(LOCAL_PATH)/generate-ck71ir.py
	python3 $(PRIVATE_GENERATOR) "$(PRIVATE_DCC)" "$@"
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $(BUILD_SHARED_LIBRARY)
endif
