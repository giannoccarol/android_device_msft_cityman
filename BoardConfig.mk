#
# Copyright (C) 2015 The Android Open-Source Project
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

DEVICE_PATH := device/msft/cityman

BUILD_BROKEN_DUP_RULES := true
# Qualcomm blobs read legacy property names (sys.listeners., radio.atfwd., net.r_rmnet, ...)
BUILD_BROKEN_VENDOR_PROPERTY_NAMESPACE := true
BUILD_BROKEN_USES_BUILD_COPY_HEADERS := true
# Android 12+: the vendor tree copies ELF blobs with PRODUCT_COPY_FILES.
BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true

# talkman: the current Lumia TWRP may still report the 950 codename.
# angler: the Android4Lumia950 TWRP 3.7.0_9-0 is built from the Nexus 6P tree.
TARGET_OTA_ASSERT_DEVICE := cityman,talkman,angler

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a53

# Second architecture
TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53

TARGET_USES_AOSP := true

# Bootloader
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true
TARGET_BOARD_PLATFORM := msm8992

# Qualcomm support: vendor/lineage maps msm8992 to hardware/qcom-caf/msm8994
# (display, audio, media), as on the MSM8992 libra LineageOS 20 tree.
BOARD_USES_QCOM_HARDWARE := true

# memfd_create is backported in the 3.10 kernel.
TARGET_HAS_MEMFD_BACKPORT := true

# Keep the device copy of dataservices (rmnetctl), the one LTE data runs
# with on the 3.10 kernel, instead of vendor/qcom/opensource/dataservices.
USE_DEVICE_SPECIFIC_DATASERVICES := true

WITH_DEXPREOPT := true
DONT_DEXPREOPT_PREBUILTS := true

TARGET_USE_AOSP_SURFACEFLINGER := true

# kernel
BOARD_KERNEL_BASE        := 0x00000000
BOARD_KERNEL_PAGESIZE    := 4096
BOARD_KERNEL_TAGS_OFFSET := 0x01E00000
BOARD_RAMDISK_OFFSET     := 0x02000000
BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.hardware=cityman boot_cpus=0-3
BOARD_KERNEL_CMDLINE += lpm_levels.sleep_disabled=1 msm_poweroff.download_mode=0
BOARD_KERNEL_CMDLINE += loop.max_part=7 androidboot.boot_devices=soc.0/f9824900.sdhci
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
# 1 MB kernel log while Android 16 is being brought up
BOARD_KERNEL_CMDLINE += log_buf_len=1M
# No Lineage AudioFX: it runs as a persistent app
TARGET_EXCLUDES_AUDIOFX := true
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset $(BOARD_RAMDISK_OFFSET) --tags_offset $(BOARD_KERNEL_TAGS_OFFSET)
#KERNEL_TOOLCHAIN := $(shell pwd)/prebuilts/arm64-gcc/bin
#KERNEL_TOOLCHAIN_PREFIX := aarch64-elf-
# Kernel: giannoccarol/android_kernel_mmo_msm8994, branch
# lineage-20-cityman-camera (mmo_defconfig), based on lineage-20-cityman.
# Camera changes are integrated selectively on the Android 13 baseline.
# Image.gz-dtb carries the
# talkman and cityman DTBs; LK2ND selects cityman (androidboot.dtb_idx=1).
# boot_cpus=0-3 starts only the A53 cluster; init.cityman.power.sh brings
# cpu4-7 online after the A57 policy is set. The kernel carries the cityman
# KTM limits (cityman-thermal.dtsi). Eight cores are untested on the phone.
TARGET_KERNEL_SOURCE := kernel/mmo/msm8994
TARGET_KERNEL_CONFIG := mmo_defconfig
TARGET_KERNEL_CLANG_COMPILE := false
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
TARGET_COMPILE_WITH_MSM_KERNEL := true
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64

# Kernel - prebuilt
#TARGET_FORCE_PREBUILT_KERNEL := true
#ifeq ($(TARGET_FORCE_PREBUILT_KERNEL),true)
#TARGET_PREBUILT_KERNEL := $(DEVICE_PATH)/prebuilts/kernel
#endif

# APEX

# Audio
BOARD_USES_ALSA_AUDIO := true
AUDIO_FEATURE_ENABLED_MULTI_VOICE_SESSIONS := true
AUDIO_FEATURE_ENABLED_SPKR_PROTECTION := false
USE_XML_AUDIO_POLICY_CONF := 1


# Bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_QCOM := true
# The prebuilt wcnss_filter uses the wc_transport.* properties without
# the vendor. prefix, like init.cityman.rc.
QCOM_BT_USE_OLD_WCNSS_FILTER := true
BOARD_HAS_QCA_BT_ROME := true
WCNSS_FILTER_USES_SIBS := true

# Camera
BOARD_QTI_CAMERA_32BIT_ONLY := true
TARGET_PROCESS_SDK_VERSION_OVERRIDE += \
    /vendor/bin/mm-qcamera-daemon=27

# Charger
BOARD_CHARGER_ENABLE_SUSPEND := true

# Display
BOARD_EGL_CFG := $(DEVICE_PATH)/configs/egl.cfg
MAX_EGL_CACHE_KEY_SIZE := 12*1024
MAX_EGL_CACHE_SIZE := 2048*1024
USE_OPENGL_RENDERER := true
TARGET_USES_ION := true
TARGET_USES_C2D_COMPOSITION := true
TARGET_USES_GRALLOC1_ADAPTER := true
TARGET_USES_HWC2 := true
TARGET_ADDITIONAL_GRALLOC_10_USAGE_BITS := 0x2000U | 0x02000000U
TARGET_DISABLE_POSTRENDER_CLEANUP := true
HAVE_ADRENO_SOURCE:= false

# Dexpreopt (Enable dex-preoptimization to speed up first boot sequence)
ifeq ($(HOST_OS),linux)
  ifneq ($(TARGET_BUILD_VARIANT),eng)
    ifeq ($(WITH_DEXPREOPT),)
      WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY := false
      WITH_DEXPREOPT := true
    endif
  endif
endif

# GPS
BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE := $(TARGET_BOARD_PLATFORM)
BOARD_VENDOR_QCOM_LOC_PDK_FEATURE_SET := true
TARGET_NO_RPC := true

# HIDL
PRODUCT_ENFORCE_VINTF_MANIFEST_OVERRIDE := true
# Not an A/B device: since Android 16 an unset AB_OTA_UPDATER means true and
# the OTA step then asks for META/ab_partitions.txt.
AB_OTA_UPDATER := false

# Non-A/B: LineageOS defaults to copying the full recovery image (24 MB) to
# vendor/etc/recovery.img, which overflows the 248 MB vendor partition. Ship
# the recovery-from-boot patch instead (about 12 MB). It is applied only if
# persist.vendor.recovery_update is set, so TWRP stays in place by default.
BOARD_USES_FULL_RECOVERY_IMAGE := false

DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += $(DEVICE_PATH)/framework_compatibility_matrix.xml
DEVICE_MATRIX_FILE := $(DEVICE_PATH)/compatibility_matrix.xml
TARGET_FS_CONFIG_GEN += $(DEVICE_PATH)/config.fs

# Partitions
TARGET_USERIMAGES_USE_EXT4 := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 33554432
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3221225472
BOARD_SYSTEMIMAGE_JOURNAL_SIZE := 0
BOARD_USERDATAIMAGE_PARTITION_SIZE := 11649679360
# Android mounts the cache partition as /metadata (fstab.cityman); recovery and
# the OTA tools still see it as /cache (recovery.fstab) and size blocks from it.
BOARD_CACHEIMAGE_PARTITION_SIZE := 100663296
# Creates the /metadata mount point in the system root: first stage init
# mounts the cache partition there and moves the mount on switch_root.
BOARD_USES_METADATA_PARTITION := true
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_PARTITION_SIZE := 260046848
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
# About 1400 files: the default (one inode per block) spends ~15 MB of the
# 248 MB partition on inode tables.
BOARD_VENDORIMAGE_EXTFS_INODE_COUNT := 4096
BOARD_FLASH_BLOCK_SIZE := 131072
TARGET_COPY_OUT_VENDOR := vendor

BOARD_USES_SECURE_SERVICES := true
BOARD_ROOT_EXTRA_FOLDERS := persist firmware
BOARD_ROOT_EXTRA_SYMLINKS := /vendor/bt_firmware:/bt_firmware

# Netd
TARGET_OMIT_NETD_TETHER_FTP_HELPER := true

# Peripheral manager
TARGET_PER_MGR_ENABLED := true

# Power
TARGET_USES_INTERACTION_BOOST := true
TARGET_USES_NON_LEGACY_POWERHAL := true
# Power hints through perf locks: the OnePlus 2 perfd client has no perf_hint
$(call soong_config_set,qtipower,mode_ext_lib,libpower_ext_cityman)

BOARD_SUPPRESS_SECURE_ERASE := true
TARGET_RECOVERY_FSTAB = $(DEVICE_PATH)/rootdir/etc/recovery.fstab

# Properties
TARGET_PRODUCT_PROP += $(DEVICE_PATH)/product.prop

# Releasetools
TARGET_RELEASETOOLS_EXTENSIONS := $(DEVICE_PATH)

# SELinux
BOARD_SEPOLICY_DIRS += \
    $(DEVICE_PATH)/sepolicy
SELINUX_IGNORE_NEVERALLOWS := true

# Android 16 has no LD_SHIM_LIBS: the blobs that need a shim are patched to
# load it (vendor/msft/cityman/README-cityman-a16.md).



# Telephony
TARGET_USES_ALTERNATIVE_MANUAL_NETWORK_SELECT := true

# Wifi
BOARD_HAS_QCOM_WLAN := true
BOARD_WLAN_DEVICE := qcwcn
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
WIFI_DRIVER_FW_PATH_STA := "sta"
WIFI_DRIVER_FW_PATH_AP  := "ap"
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
# The AIDL Wi-Fi HAL replaces the static chip modes with the driver's dynamic
# combination (kV3) after configuring the chip in mode 0 (kV1Sta), then
# rejects every request with "Chip not configured in a mode yet". Declare the
# STA/AP combination explicitly, as on the OnePlus 2 tree.
WIFI_HAL_INTERFACE_COMBINATIONS := {{{STA}, 1}}, {{{AP}, 1}}
WIFI_AVOID_IFACE_RESET_MAC_CHANGE := true


# NFC
BOARD_NFC_CHIPSET := pn547
BOARD_NFC_HAL_SUFFIX := msm8992
BOARD_NFC_DEVICE := "/dev/pn547"

-include vendor/msft/cityman/BoardConfigVendor.mk
