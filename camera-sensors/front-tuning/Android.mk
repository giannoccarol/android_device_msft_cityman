# Front camera chromatix built from C: the vendor imx179_liteon VFE/3A
# payloads with brighter AEC targets (see gen-front-chromatix.py), in three
# colour matrix sets, and optionally the common payload with this module's
# black level.  The container and struct layout are the rear tuning's.
#
#   libchromatix_vs6955_imx179_<mode>  the donor's colour matrices
#   libchromatix_vs6955_swarc_<mode>   DCC ISP_SW_ARC matrix in all entries
#   libchromatix_vs6955_hwqc_<mode>    DCC ISP_HW_QC matrix in all entries
#   libchromatix_vs6955_common         only with CITYMAN_CAMERA_FRONT_BLACK
#
# The DCC matrices are written by gen-front-ccm.py into the build output,
# never into the tree.  Which set the camera loads is chosen in
# ../vs6955/Android.mk (CITYMAN_CAMERA_FRONT_CCM) and at runtime with
# vendor.camera.front.ccm.  Options: README.md and ../vs6955/README.md.
LOCAL_PATH := $(call my-dir)

ifeq ($(CITYMAN_CAMERA_FRONT_VS6955),true)
ifeq ($(strip $(CITYMAN_CAMERA_FRONT_DCC)),)
$(error front-tuning requires CITYMAN_CAMERA_FRONT_DCC pointing to the pinned private 0A214000.dcc)
endif

# mean (default) or 0..5: which of the six DCC matrices.  Part of the
# generated file name, so changing it regenerates the header.
cityman_front_ccm_index := $(or $(strip $(CITYMAN_CAMERA_FRONT_CCM_INDEX)),mean)
ifeq ($(filter mean 0 1 2 3 4 5,$(cityman_front_ccm_index)),)
$(error CITYMAN_CAMERA_FRONT_CCM_INDEX must be mean or 0..5, not $(cityman_front_ccm_index))
endif

cityman_front_cflags := -Os -Wall -Werror -fPIC

# $(1) = matrix set, $(2) = mode
define cityman-front-chromatix
include $$(CLEAR_VARS)
LOCAL_MODULE               := libchromatix_vs6955_$(1)_$(2)
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := chromatix_front.c
LOCAL_C_INCLUDES           := $$(LOCAL_PATH) $$(LOCAL_PATH)/../rear-tuning/chromatix
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := $$(cityman_front_cflags) -DCHX_TYPE=chx_vfe_t \
                              -DCHX_DATA=\"data/chx_front_$(2).inc\"
ifneq ($(1),imx179)
cityman_front_ccm_header := $$(call local-generated-sources-dir)/front_ccm_$(1)_$$(cityman_front_ccm_index).h
LOCAL_GENERATED_SOURCES    += $$(cityman_front_ccm_header)
LOCAL_C_INCLUDES           += $$(dir $$(cityman_front_ccm_header))
LOCAL_CFLAGS               += -DCITYMAN_FRONT_CCM_HEADER=\"$$(notdir $$(cityman_front_ccm_header))\"
$$(cityman_front_ccm_header): PRIVATE_DCC := $$(CITYMAN_CAMERA_FRONT_DCC)
$$(cityman_front_ccm_header): PRIVATE_GENERATOR := $$(LOCAL_PATH)/gen-front-ccm.py
$$(cityman_front_ccm_header): PRIVATE_ARGS := $(1) $$(cityman_front_ccm_index)
$$(cityman_front_ccm_header): $$(CITYMAN_CAMERA_FRONT_DCC) $$(LOCAL_PATH)/gen-front-ccm.py
	python3 $$(PRIVATE_GENERATOR) "$$(PRIVATE_DCC)" $$(PRIVATE_ARGS) "$$@"
endif
LOCAL_LDFLAGS              := -nostdlib -Wl,-soname,libchromatix_vs6955_$(1)_$(2).so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $$(BUILD_SHARED_LIBRARY)
endef
$(foreach s,imx179 swarc hwqc,$(foreach n,preview video_binning liveshot,\
  $(eval $(call cityman-front-chromatix,$(s),$(n)))))

# Black level: "V" (all four channels) or "R Gr Gb B", 12-bit.  Empty (the
# default) builds nothing and the camera keeps the vendor imx179_liteon_common.
cityman_front_black := $(strip $(CITYMAN_CAMERA_FRONT_BLACK))
ifneq ($(cityman_front_black),)
ifeq ($(words $(cityman_front_black)),1)
cityman_front_black := $(cityman_front_black) $(cityman_front_black) $(cityman_front_black) $(cityman_front_black)
endif
ifneq ($(words $(cityman_front_black)),4)
$(error CITYMAN_CAMERA_FRONT_BLACK must be one value or four (R Gr Gb B), not "$(CITYMAN_CAMERA_FRONT_BLACK)")
endif
include $(CLEAR_VARS)
LOCAL_MODULE               := libchromatix_vs6955_common
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := chromatix_front.c
LOCAL_C_INCLUDES           := $(LOCAL_PATH) $(LOCAL_PATH)/../rear-tuning/chromatix
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := $(cityman_front_cflags) -DCHX_TYPE=chx_common_t \
                              -DCHX_DATA=\"data/chx_front_common.inc\" \
                              -DCITYMAN_FRONT_BLACK_R=$(word 1,$(cityman_front_black)) \
                              -DCITYMAN_FRONT_BLACK_GR=$(word 2,$(cityman_front_black)) \
                              -DCITYMAN_FRONT_BLACK_GB=$(word 3,$(cityman_front_black)) \
                              -DCITYMAN_FRONT_BLACK_B=$(word 4,$(cityman_front_black))
LOCAL_LDFLAGS              := -nostdlib -Wl,-soname,libchromatix_vs6955_common.so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $(BUILD_SHARED_LIBRARY)
endif
endif
