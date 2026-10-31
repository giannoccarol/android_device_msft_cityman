# libflash_hill: the camera daemon's userspace driver for the rear flash IC
# ("Hill", 8-bit 0x60 on CCI master 0). flash_lib_hill.c is hraj9258's, see
# README.md. Loaded as libflash_<flash_name>.so from the rear sensor library;
# installed by device.mk with CITYMAN_CAMERA_FLASH.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE               := libflash_hill
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := flash_lib_hill.c
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := -Os -Wall -Werror -fPIC -ffreestanding
LOCAL_LDFLAGS              := -nostdlib -Wl,-soname,libflash_hill.so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $(BUILD_SHARED_LIBRARY)
