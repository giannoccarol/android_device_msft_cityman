LOCAL_PATH := $(call my-dir)

ifeq ($(CITYMAN_CAMERA_AF),true)
ifeq ($(strip $(CITYMAN_CAMERA_AF_INF_CODE)),)
$(error CITYMAN_CAMERA_AF=true needs CITYMAN_CAMERA_AF_INF_CODE from the module NVM)
endif
ifeq ($(strip $(CITYMAN_CAMERA_AF_MACRO_CODE)),)
$(error CITYMAN_CAMERA_AF=true needs CITYMAN_CAMERA_AF_MACRO_CODE from the module NVM)
endif
carrera_af_name := $(or $(CITYMAN_CAMERA_AF_NAME),carrera)
carrera_af_cflags := -std=gnu11 -Wall -Wextra -Werror -fvisibility=hidden
# The daemon keeps pointers into these libraries after dlclose(): keep them mapped.
carrera_af_ldflags := -Wl,-z,nodelete

include $(CLEAR_VARS)
LOCAL_MODULE := libactuator_carrera
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := actuator_carrera.c
LOCAL_CFLAGS := $(carrera_af_cflags) \
    -DCARRERA_AF_NAME=\"$(carrera_af_name)\" \
    -DCARRERA_AF_INF_CODE=$(CITYMAN_CAMERA_AF_INF_CODE) \
    -DCARRERA_AF_MACRO_CODE=$(CITYMAN_CAMERA_AF_MACRO_CODE) \
    -DCARRERA_AF_OIS_INIT=$(if $(filter false,$(CITYMAN_CAMERA_AF_OIS_INIT)),0,1)
LOCAL_C_INCLUDES := $(TARGET_KERNEL_SOURCE)/include
LOCAL_LDFLAGS := $(carrera_af_ldflags)
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $(BUILD_SHARED_LIBRARY)

define carrera-af-tuning
include $$(CLEAR_VARS)
LOCAL_MODULE := libactuator_carrera_$(1)
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES := af_tuning_forward.c
LOCAL_CFLAGS := $$(carrera_af_cflags) -DCARRERA_AF_SYMBOL_NAME=carrera \
    -DCARRERA_AF_MODE=$(1)
LOCAL_SHARED_LIBRARIES := libdl
LOCAL_LDFLAGS := $$(carrera_af_ldflags)
LOCAL_VENDOR_MODULE := true
LOCAL_32_BIT_ONLY := true
include $$(BUILD_SHARED_LIBRARY)
endef
$(eval $(call carrera-af-tuning,camera))
$(eval $(call carrera-af-tuning,camcorder))
endif
