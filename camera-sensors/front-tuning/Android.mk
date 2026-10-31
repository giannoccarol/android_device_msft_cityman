# Front camera exposure chromatix built from C: the vendor imx179_liteon
# VFE/3A payloads with brighter AEC targets (see gen-front-chromatix.py).
# The container and struct layout are the rear tuning's.
LOCAL_PATH := $(call my-dir)

ifeq ($(CITYMAN_CAMERA_FRONT_VS6955),true)
define cityman-front-chromatix
include $$(CLEAR_VARS)
LOCAL_MODULE               := libchromatix_vs6955_$(1)
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := ../rear-tuning/chromatix/chromatix_lib.c
LOCAL_C_INCLUDES           := $$(LOCAL_PATH) $$(LOCAL_PATH)/../rear-tuning/chromatix
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := -Os -Wall -Werror -fPIC -DCHX_TYPE=chx_vfe_t \
                              -DCHX_DATA=\"data/chx_front_$(1).inc\"
LOCAL_LDFLAGS              := -nostdlib -Wl,-soname,libchromatix_vs6955_$(1).so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $$(BUILD_SHARED_LIBRARY)
endef
$(foreach n,preview video_binning liveshot,$(eval $(call cityman-front-chromatix,$(n))))
endif
