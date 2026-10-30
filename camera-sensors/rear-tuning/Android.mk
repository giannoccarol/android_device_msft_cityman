# Rear camera tuning for cityman, built from C: chromatix (ISP/3A), AF search
# tuning and the lens actuator. Taken from hraj9258's talkman port, see
# README.md. Installed by device.mk with CITYMAN_CAMERA_SMIA_EXPERIMENTAL.
LOCAL_PATH := $(call my-dir)

cityman_rear_cflags  := -Os -Wall -Werror -fPIC
cityman_rear_ldflags := -nostdlib

# -------------------------------------------------------------- actuator ----
include $(CLEAR_VARS)
LOCAL_MODULE               := libactuator_cityman
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := actuator_lib_cityman.c
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := $(cityman_rear_cflags)
LOCAL_LDFLAGS              := $(cityman_rear_ldflags) -Wl,-soname,libactuator_cityman.so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $(BUILD_SHARED_LIBRARY)

# ------------------------------------------------------- AF search tuning ----
# af_actuator_init() dlopens libactuator_<name>_<mode>.so for both modes.
define cityman-af-algo
include $$(CLEAR_VARS)
LOCAL_MODULE               := libactuator_cityman_$(1)
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := af/af_tuning_lib.c
LOCAL_C_INCLUDES           := $$(LOCAL_PATH)/af
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := $$(cityman_rear_cflags) \
                              -DAF_MODE=$(1) -DAF_DATA=\"data/af_$(1).inc\"
LOCAL_LDFLAGS              := $$(cityman_rear_ldflags) \
                              -Wl,-soname,libactuator_cityman_$(1).so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $$(BUILD_SHARED_LIBRARY)
endef
$(foreach m,camera camcorder,$(eval $(call cityman-af-algo,$(m))))

# ------------------------------------------------------------- chromatix ----
# Exactly the names camera-sensors/imx230/sensor_lib_imx230.c references.
cityman_chx_names := common snapshot default_video liveshot postproc \
                     cpp_preview cpp_snapshot cpp_video cpp_uhd_video cpp_liveshot

cityman_chx_type_common        := chx_common_t
cityman_chx_type_snapshot      := chx_vfe_t
cityman_chx_type_default_video := chx_vfe_t
cityman_chx_type_liveshot      := chx_vfe_t
cityman_chx_type_postproc      := chx_postproc_t
cityman_chx_type_cpp_preview   := chx_cpp_t
cityman_chx_type_cpp_snapshot  := chx_cpp_t
cityman_chx_type_cpp_video     := chx_cpp_t
cityman_chx_type_cpp_uhd_video := chx_cpp_t
cityman_chx_type_cpp_liveshot  := chx_cpp_t

cityman_chx_data_common        := chx_common
cityman_chx_data_snapshot      := chx_snapshot
cityman_chx_data_default_video := chx_default_video
cityman_chx_data_liveshot      := chx_default_video
cityman_chx_data_postproc      := chx_postproc
cityman_chx_data_cpp_preview   := chx_cpp_preview
cityman_chx_data_cpp_snapshot  := chx_cpp_snapshot
cityman_chx_data_cpp_video     := chx_cpp_video
cityman_chx_data_cpp_uhd_video := chx_cpp_snapshot
cityman_chx_data_cpp_liveshot  := chx_cpp_snapshot

define cityman-chromatix
include $$(CLEAR_VARS)
LOCAL_MODULE               := libchromatix_imx230_$(1)
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE        := true
LOCAL_SRC_FILES            := chromatix/chromatix_lib.c
LOCAL_C_INCLUDES           := $$(LOCAL_PATH)/chromatix
LOCAL_MULTILIB             := 32
LOCAL_CFLAGS               := $$(cityman_rear_cflags) \
                              -DCHX_TYPE=$$(cityman_chx_type_$(1)) \
                              -DCHX_DATA=\"data/$$(cityman_chx_data_$(1)).inc\"
LOCAL_LDFLAGS              := $$(cityman_rear_ldflags) \
                              -Wl,-soname,libchromatix_imx230_$(1).so
LOCAL_SYSTEM_SHARED_LIBRARIES := none
LOCAL_NO_CRT               := true
LOCAL_SANITIZE             := never
include $$(BUILD_SHARED_LIBRARY)
endef
$(foreach n,$(cityman_chx_names),$(eval $(call cityman-chromatix,$(n))))
