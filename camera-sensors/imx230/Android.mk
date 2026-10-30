LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libmmcamera_imx230
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := sensor_lib_imx230.c
LOCAL_CFLAGS := -std=gnu11 -Wall -Wextra -Werror -fvisibility=hidden
LOCAL_C_INCLUDES := $(TARGET_KERNEL_SOURCE)/include
cityman_sensor_profile := $(or $(CITYMAN_CAMERA_SENSOR_PROFILE),smia9)
ifeq ($(cityman_sensor_profile),nubia24)
ifeq ($(strip $(CITYMAN_CAMERA_NUBIA_SENSOR_BLOB)),)
$(error nubia24 requires CITYMAN_CAMERA_NUBIA_SENSOR_BLOB pointing to the pinned private reference)
endif
LOCAL_CFLAGS += -DCITYMAN_CAMERA_NUBIA24
cityman_sensor_header := $(call local-generated-sources-dir)/nubia24_registers.generated.h
LOCAL_GENERATED_SOURCES += $(cityman_sensor_header)
LOCAL_C_INCLUDES += $(dir $(cityman_sensor_header))
$(cityman_sensor_header): PRIVATE_BLOB := $(CITYMAN_CAMERA_NUBIA_SENSOR_BLOB)
$(cityman_sensor_header): PRIVATE_GENERATOR := $(LOCAL_PATH)/generate-nubia24.py
$(cityman_sensor_header): $(CITYMAN_CAMERA_NUBIA_SENSOR_BLOB) $(LOCAL_PATH)/generate-nubia24.py \
        $(LOCAL_PATH)/inspect-nubia-registers.py $(LOCAL_PATH)/smia_registers.h
	python3 $(PRIVATE_GENERATOR) "$(PRIVATE_BLOB)" "$@"
else ifeq ($(cityman_sensor_profile),lumia20)
ifeq ($(strip $(CITYMAN_CAMERA_LUMIA20_DCC)),)
$(error lumia20 requires CITYMAN_CAMERA_LUMIA20_DCC pointing to the pinned private 0AEACA05.dcc)
endif
LOCAL_CFLAGS += -DCITYMAN_CAMERA_LUMIA20
cityman_sensor_header := $(call local-generated-sources-dir)/lumia20_registers.generated.h
LOCAL_GENERATED_SOURCES += $(cityman_sensor_header)
LOCAL_C_INCLUDES += $(dir $(cityman_sensor_header))
$(cityman_sensor_header): PRIVATE_DCC := $(CITYMAN_CAMERA_LUMIA20_DCC)
$(cityman_sensor_header): PRIVATE_GENERATOR := $(LOCAL_PATH)/generate-lumia20.py
$(cityman_sensor_header): $(CITYMAN_CAMERA_LUMIA20_DCC) $(LOCAL_PATH)/generate-lumia20.py
	python3 $(PRIVATE_GENERATOR) "$(PRIVATE_DCC)" "$@"
else ifneq ($(cityman_sensor_profile),smia9)
$(error Unsupported CITYMAN_CAMERA_SENSOR_PROFILE=$(cityman_sensor_profile))
endif
ifeq ($(CITYMAN_CAMERA_AF),true)
# The default name pairs with camera-sensors/rear-tuning (libactuator_cityman);
# CITYMAN_CAMERA_AF_NAME=carrera selects the older camera-sensors/carrera-af.
LOCAL_CFLAGS += -DCITYMAN_CAMERA_AF \
    -DCITYMAN_CAMERA_AF_NAME=\"$(or $(CITYMAN_CAMERA_AF_NAME),cityman)\"
endif
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $(BUILD_SHARED_LIBRARY)
