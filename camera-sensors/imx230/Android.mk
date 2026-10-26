LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libmmcamera_imx230
LOCAL_SRC_FILES := sensor_lib_imx230.c
LOCAL_CFLAGS := -std=gnu11 -Wall -Wextra -Werror -fvisibility=hidden
LOCAL_C_INCLUDES := $(TARGET_KERNEL_SOURCE)/include
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $(BUILD_SHARED_LIBRARY)
