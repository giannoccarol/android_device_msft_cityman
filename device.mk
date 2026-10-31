#
# Copyright (C) 2016 The Android Open-Source Project
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

#
# Everything in this directory will become public

# Boot animation
TARGET_SCREEN_HEIGHT := 2560
TARGET_SCREEN_WIDTH := 1440

# 4 KB pages: the Qualcomm blobs are not 16 KB aligned and never need to be.
PRODUCT_CHECK_PREBUILT_MAX_PAGE_SIZE := false

# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)

# Shipping API level of the angler blobs. Without it Android 16 treats the
# device as launching on 16 and drops hwservicemanager, vndservicemanager,
# the HIDL allocator/memory and the OMX service, which every HIDL HAL here needs.
$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_m.mk)

# Display
TARGET_SCREEN_DENSITY := 564

# Screen density
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxxhdpi
PRODUCT_AAPT_PREBUILT_DPI := xxxhdpi xxhdpi xhdpi hdpi

# Dexpreopt
WITH_DEXPREOPT := true

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.camera.full.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.camera.full.xml \
    frameworks/native/data/etc/android.hardware.camera.raw.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.camera.raw.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.wifi.passpoint.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.ambient_temperature.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.ambient_temperature.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.sensor.hifi_sensors.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.software.sip.voip.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.sip.voip.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.audio.low_latency.xml \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.audio.pro.xml \
    frameworks/native/data/etc/android.hardware.telephony.cdma.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.telephony.cdma.xml \
    frameworks/native/data/etc/android.hardware.telephony.ims.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.telephony.ims.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.nfc.hcef.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.android.nfc_extras.xml  \
    frameworks/native/data/etc/android.hardware.ethernet.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.software.midi.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/android.software.verified_boot.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.verified_boot.xml \
    frameworks/native/data/etc/com.nxp.mifare.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/com.nxp.mifare.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-0.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.vulkan.level.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.vulkan.version.xml

# APEX
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/ld.config.txt:$(TARGET_COPY_OUT_SYSTEM)/etc/swcodec/ld.config.txt


    
# Audio tools (userdebug)
PRODUCT_PACKAGES_DEBUG += \
    tinycap \
    tinymix \
    tinypcminfo \
    tinyplay

# Soong namespaces of the msm8994 qcom-caf HALs. Audio stays on the generic
# hardware/qcom/audio HAL (Android.mk), so its caf namespace is not exported.
PRODUCT_SOONG_NAMESPACES += \
    hardware/qcom-caf/msm8994/display \
    hardware/qcom-caf/msm8994/media \
    hardware/qcom-caf/bt/libbt-vendor

# The msm8994 qcom-caf display HALs are named after msm8994; the board is msm8992.
PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.copybit=msm8994 \
    ro.hardware.gralloc=msm8994 \
    ro.hardware.hwcomposer=msm8994 \
    ro.hardware.memtrack=msm8994

# qcacld unloads itself when Wi-Fi goes down and loads again only on a
# firmware path write: make the Wi-Fi HAL write it for our chip mode too
# (patch hardware/interfaces/0001 in talkman-manifests lineageos-23).
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.wifi.fw_path_any_mode=true

# Audio
PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-impl \
    android.hardware.audio.service \
    android.hardware.audio@6.0 \
    android.hardware.audio@6.0-impl \
    android.hardware.soundtrigger@2.0-impl \
    android.hardware.audio.effect@2.0-impl \
    android.hardware.audio.effect@6.0 \
    android.hardware.audio.effect@6.0-impl \
    audio.primary.msm8992 \
    audio.bluetooth.default \
    android.hardware.bluetooth.audio@2.0-impl \
    audio.usb.default \
    audio.r_submix.default \
    libaudio-resampler \
    libqcomvoiceprocessing \
    libqcomvoiceprocessingdescriptors \
    libqcomvisualizer \
    libqcompostprocbundle \
    libvolumelistener

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml \
    $(LOCAL_PATH)/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths.xml \
    $(LOCAL_PATH)/audio/audio_platform_info.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info.xml \
    $(LOCAL_PATH)/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/audio_policy_volumes_drc.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/audio_policy_volumes_drc.xml \
    $(LOCAL_PATH)/audio/sound_trigger_mixer_paths.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/sound_trigger_mixer_paths.xml \
    $(LOCAL_PATH)/audio/sound_trigger_platform_info.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/sound_trigger_platform_info.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_audio_policy_configuration.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/bluetooth_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/usb_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/default_volume_tables.xml \

# Bluetooth HAL
PRODUCT_PACKAGES += \
    libbt-vendor \
    android.hardware.bluetooth@1.0-impl \
    android.hardware.bluetooth@1.0-service

# Radio: IRadio 1.4 / IRadioConfig 1.1 wrappers over rild (radio/README.md)
PRODUCT_PACKAGES += \
    android.hardware.radio@1.4-service.cityman \
    android.hardware.radio.config@1.1-service.cityman

PRODUCT_PROPERTY_OVERRIDES += \
    ro.bt.bdaddr_path=/persist/bdaddr.txt

# Charger
PRODUCT_PACKAGES += \
    charger_res_images

# Camera
# Enable in a dedicated camera image after offline ABI checks and phone-test
# coordination. The module can always be built explicitly with m.
ifeq ($(CITYMAN_CAMERA_SMIA_EXPERIMENTAL),true)
PRODUCT_PACKAGES += libmmcamera_imx230
# Rear tuning built from C (camera-sensors/rear-tuning, from hraj9258's
# talkman port): IMX230 chromatix with this module's black levels and colour
# matrix, AF search tuning, and the lens actuator (high byte first).
PRODUCT_PACKAGES += \
    libchromatix_imx230_common \
    libchromatix_imx230_snapshot \
    libchromatix_imx230_default_video \
    libchromatix_imx230_liveshot \
    libchromatix_imx230_postproc \
    libchromatix_imx230_cpp_preview \
    libchromatix_imx230_cpp_snapshot \
    libchromatix_imx230_cpp_video \
    libchromatix_imx230_cpp_uhd_video \
    libchromatix_imx230_cpp_liveshot
# The lumia20 profile's second mode: 2672x1872 2x2-binned at 30 fps, picked
# by the daemon for video (camera-sensors/imx230/LUMIA20-BINNED.md).
ifeq ($(CITYMAN_CAMERA_SENSOR_PROFILE),lumia20)
CITYMAN_CAMERA_LUMIA20_BINNED ?= true
endif
# Rear AF (needs a kernel with actuator0 and ACTUATOR_VCM_BE). The default
# actuator is rear-tuning's "cityman"; "carrera" is the older library, whose
# codes (INF 909, the NVM mid point 1178) cover only half of the lens travel.
CITYMAN_CAMERA_AF ?= true
CITYMAN_CAMERA_AF_NAME ?= cityman
CITYMAN_CAMERA_AF_INF_CODE ?= 909
CITYMAN_CAMERA_AF_MACRO_CODE ?= 1178
ifeq ($(CITYMAN_CAMERA_AF),true)
PRODUCT_PACKAGES += \
    libactuator_$(CITYMAN_CAMERA_AF_NAME) \
    libactuator_$(CITYMAN_CAMERA_AF_NAME)_camera \
    libactuator_$(CITYMAN_CAMERA_AF_NAME)_camcorder
endif
# Rear flash: the Hill IC on CCI master 0 (camera-sensors/hill-flash). Safe
# with a kernel that lacks the Hill node: the daemon then finds no flash.
CITYMAN_CAMERA_FLASH ?= true
ifeq ($(CITYMAN_CAMERA_FLASH),true)
PRODUCT_PACKAGES += libflash_hill
endif
endif
# Front VS6955CA probe module. Requires the private 0A214000.dcc and a kernel
# with qcom,camera@1 enabled; off by default.
ifeq ($(CITYMAN_CAMERA_FRONT_VS6955),true)
PRODUCT_PACKAGES += libmmcamera_vs6955
# its exposure chromatix, built from C in camera-sensors/front-tuning
PRODUCT_PACKAGES += \
    libchromatix_vs6955_preview \
    libchromatix_vs6955_video_binning \
    libchromatix_vs6955_liveshot
endif
PRODUCT_PACKAGES += \
    libcamera_delayfree \
    camera.msm8992 \
    libmmcamera_interface \
    libmmjpeg_interface \
    libqomx_core \
    android.hardware.camera.provider@2.4-impl \
    android.hardware.camera.provider@2.4-service \
    camera.device@1.0-impl \
    camera.device@3.2-impl

PRODUCT_PACKAGES += \
    vendor.qti.hardware.camera.device@1.0

# Display
PRODUCT_PACKAGES += \
    gralloc.msm8994 \
    copybit.msm8994 \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl \
    hwcomposer.msm8994 \
    memtrack.msm8994 \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service

PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.disable_backpressure=1 \
    debug.sf.enable_gl_backpressure=1

# Performance tuning taken from the LineageOS 20 OnePlus 2 port (MSM8994,
# same msm8994 CAF display HAL): more layers on the MDP, no SurfaceFlinger
# composition prediction or transaction tracing, SystemUI compiled with
# speed, quieter logs.
PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.enable_transaction_tracing=false \
    debug.sf.predict_hwc_composition_strategy=0 \
    dev.pm.dyn_samplingrate=1 \
    persist.hwc.mdpcomp.maxpermixer=5 \
    persist.hwc.ptor.enable=true \
    persist.hwc.pubypass=true \
    persist.mdpcomp_perfhint=50 \
    persist.metadata_dynfps.disable=true \
    vidc.dec.120fps.enabled=1 \
    dalvik.vm.heapstartsize=16m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=2m \
    dalvik.vm.heapmaxfree=8m \
    persist.log.tag.AnalyticsService=S \
    persist.log.tag.KernelCpuUidActiveTimeReader=S \
    persist.log.tag.Tracer=S \
    persist.log.tag.NearbySharing=S \
    persist.log.tag.IntervalStats=S \
    persist.log.tag.CompatibilityChangeReporter=S \
    persist.log.tag.SQLiteLog=S \
    persist.log.tag.wificond=S \
    persist.log.tag.TrafficStats=S \
    persist.log.tag.ContrastColorUtil=S \
    persist.log.tag.OpenGLRenderer=S

# DRM
PRODUCT_PACKAGES += \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.0-service \
    android.hardware.drm-service.clearkey

# Dumpstate HAL
PRODUCT_PACKAGES += \
    android.hardware.dumpstate@1.0-service.cityman

# For android_filesystem_config.h
PRODUCT_PACKAGES += \
   fs_config_files

PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-service.software

# General support
PRODUCT_PACKAGES += \
    libtinyxml

# GNSS HAL — cityman impl ignores CAF NMEA length (nmeaCb SIGABRT)
PRODUCT_PACKAGES += \
    android.hardware.gnss@1.0-impl.cityman \
    android.hardware.gnss@1.0-service

# GPS
PRODUCT_PACKAGES += \
    libgps.utils \
    gps.msm8992

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/sec_config:$(TARGET_COPY_OUT_SYSTEM)/etc/sec_config \
    $(LOCAL_PATH)/gps/gps.conf:$(TARGET_COPY_OUT_SYSTEM)/etc/gps.conf \
    $(LOCAL_PATH)/gps/gps_debug.conf:$(TARGET_COPY_OUT_SYSTEM)/etc/gps_debug.conf

# Health
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-impl \
    android.hardware.health@2.1-service

# HIDL
PRODUCT_PACKAGES += \
    libhidltransport \
    libhwbinder

# IMS
PRODUCT_PACKAGES += \
    ims-ext-common \
    ims_ext_common.xml \
    com.android.ims.rcsmanager

# Cityman RIL helpers, built from kernel/mmo/msm8994/tools/cityman-ril
# (the build finds that Android.mk; do not include it again here).
# The init preloads are in rootdir/etc/init.cityman.rc.
PRODUCT_PACKAGES += \
    cityman-uim-bootstrap \
    libcityman_ds_target \
    libcityman_ril_init

# init
PRODUCT_PACKAGES += \
    init.cityman.rc \
    init.cityman.usb.rc \
    init.cityman.sensors.rc \
    fstab.cityman \
    ueventd.cityman.rc \
    init.recovery.cityman.rc \
    init.cityman.ramdump.rc \
    init.cityman.diag.rc \
    init.cityman.misc.rc

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.qcom.devwait.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.qcom.devwait.sh \
    $(LOCAL_PATH)/rootdir/etc/init.qcom.devstart.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.qcom.devstart.sh \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.bootlog.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.bootlog.sh \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.modem.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.modem.sh \
    $(LOCAL_PATH)/rootdir/etc/fstab.cityman:$(TARGET_COPY_OUT_ROOT)/fstab.cityman \
    $(LOCAL_PATH)/rootdir/etc/fstab.cityman:$(TARGET_COPY_OUT_RAMDISK)/fstab.cityman
#    $(LOCAL_PATH)/rootdir/etc/init.msm8992.sensor.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.msm8992.sensor.sh

# Keylayout
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/keylayout/gpio-keys.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/keylayout/qpnp_pon.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/qpnp_pon.kl

# Keymaster HAL
PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0-impl \
    android.hardware.keymaster@3.0-service

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/keylayout/synaptics_rmi4_i2c.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/synaptics_rmi4_i2c.idc
# Light HAL
PRODUCT_PACKAGES += \
    lights.cityman \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service
    
# MBN
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.sh

# Media
PRODUCT_PACKAGES += \
    libc2dcolorconvert \
    libstagefrighthw \
    libOmxCore \
    libmm-omxcore \
    libOmxVdec \
    libOmxVenc

PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/media_codecs_google_telephony.xml \
    $(LOCAL_PATH)/configs/media_codecs.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/media_codecs.xml \
    $(LOCAL_PATH)/configs/media_codecs_performance.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/media_codecs_performance.xml \
    $(LOCAL_PATH)/configs/media_profiles.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/media_profiles.xml \
    $(LOCAL_PATH)/configs/seccomp/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy

# MSM IRQ Balancer configuration file
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/msm_irqbalance.conf:$(TARGET_COPY_OUT_VENDOR)/etc/msm_irqbalance.conf

# NFC packages
PRODUCT_PACKAGES += \
    com.android.nfc_extras \
    Tag \
    NfcNci

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/nfc/libnfc-nci.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nci.conf \
    $(LOCAL_PATH)/nfc/libnfc-nxp.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nxp.conf \
    $(LOCAL_PATH)/nfc/nfcee_access.xml:system/etc/nfcee_access.xml

# Overlay
DEVICE_PACKAGE_OVERLAYS := \
    $(LOCAL_PATH)/overlay

# Privapp Whitelist
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/privapp-permissions-cityman.xml:system/etc/permissions/privapp-permissions-cityman.xml

# Power HAL
PRODUCT_PACKAGES += \
    android.hardware.power-service-qti

# Power configuration file
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.power.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.power.sh \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.big-max-freq.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.big-max-freq.sh

# Performance profiles: Settings > Battery and a Quick Settings tile
PRODUCT_PACKAGES += \
    CitymanParts

# Qseecomd configuration file
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.cityman.qseecomd.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.cityman.qseecomd.sh

# Ril
PRODUCT_PACKAGES += \
    telephony-ext \
    qti-telephony-hidl-wrapper \
    qti_telephony_hidl_wrapper.xml \
    qti-telephony-utils \
    qti_telephony_utils.xml \
    librmnetctl \
    rmnetcli \
    libaudioclient_shim_vendor \
    slim_shim

PRODUCT_BOOT_JARS += \
    telephony-ext

PRODUCT_PROPERTY_OVERRIDES += \
    persist.radio.snapshot_enabled=1 \
    persist.radio.snapshot_timer=10

# Sensors — AP I2C HAL (not sensors.qcom / ADSP)
PRODUCT_PACKAGES += \
    sensors.cityman \
    android.hardware.sensors@1.0-impl

# Shims: libcutils_shim comes from hardware/lineage/compat.
PRODUCT_PACKAGES += \
    libcutils_shim

# Android 16 vendor namespace: only vendor libraries and LLNDK are visible to
# vendor processes, so the libraries the blobs need from the old system
# partition are installed as vendor variants (blobs patched in the vendor tree,
# see vendor/msft/cityman/README-cityman-a16.md).
PRODUCT_PACKAGES += \
    libnetutils.vendor \
    libsqlite.vendor \
    libsensor_camera_stub \
    libsensorndkbridge \
    libstdc++_vendor \
    libgps.utils \
    libloc_core \
    libcrypto_shim \
    libprotobuf-cpp-lite-3.9.1-vendorcompat \
    libutils-v33

# TimeKeep
PRODUCT_PACKAGES += \
    timekeep \
    TimeKeep

# Thermal HAL
PRODUCT_PACKAGES += \
    thermal.cityman 

# AIDL thermal HAL (reporting only; thermal-engine/KTM still throttle)
PRODUCT_PACKAGES += \
    android.hardware.thermal-service.cityman

# thermal-engine needs libqti-perfd-client.so to start, and reads the
# cityman configuration given on its command line. The vendor tree now ships
# the MSM8994 perfd with its client library (OnePlus 2 blobs), so the power
# HAL and thermal-engine reach a real daemon. The open-source stub in
# libqti-perfd-client/ stays as a fallback: add it back here (and drop the
# vendor copies) if perfd does not run.

# perfd reads the minimum frequencies and the core_ctl range.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.min_freq_0=384000 \
    ro.min_freq_4=384000 \
    ro.core_ctl_min_cpu=0 \
    ro.core_ctl_max_cpu=4

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/thermal-engine-cityman.conf:$(TARGET_COPY_OUT_VENDOR)/etc/thermal-engine-cityman.conf

# VINTF: the 3.10 kernel cannot meet the framework matrix kernel
# requirements; do not enforce them when building OTA packages.
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

# No userfaultfd in 3.10: ART runs the CC collector. With an unknown kernel
# the build would dexpreopt everything for CMC, ART would reject it all and
# odrefresh would recompile the boot image on the phone.
PRODUCT_ENABLE_UFFD_GC := false

# Drop Updater and Gallery2 (see removepackages/Android.mk)
PRODUCT_PACKAGES += \
    RemovePackages

# VNDK v29 libraries for the vendor blobs (no VNDK snapshots since Android 15)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilts/vndk-v29/arm/libprotobuf-cpp-lite.so:$(TARGET_COPY_OUT_VENDOR)/lib/libprotobuf-cpp-lite-v29.so \
    $(LOCAL_PATH)/prebuilts/vndk-v29/arm64/libprotobuf-cpp-lite.so:$(TARGET_COPY_OUT_VENDOR)/lib64/libprotobuf-cpp-lite-v29.so \
    $(LOCAL_PATH)/prebuilts/vndk-v29/arm64/libcutils.so:$(TARGET_COPY_OUT_SYSTEM)/lib64/libcutils-v29.so


# USB HAL
PRODUCT_PACKAGES += \
    android.hardware.usb@1.0-service

# OEM Unlock reporting
PRODUCT_PROPERTY_OVERRIDES += \
    ro.oem_unlock_supported=1

# Vibrator HAL: AIDL adapter over the legacy timed_output module
PRODUCT_PACKAGES += \
    android.hardware.vibrator-service.legacy \
    vibrator.default

# Wi-Fi
PRODUCT_PACKAGES += \
    android.hardware.wifi-service-lazy \
    libwpa_client \
    hostapd \
    wificond \
    wpa_supplicant \
    wpa_supplicant.conf \
    cityman_wlan_mac

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    $(LOCAL_PATH)/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf \
    $(LOCAL_PATH)/wifi/WCNSS_cfg.dat:$(TARGET_COPY_OUT_VENDOR)/firmware/wlan/qca_cld/WCNSS_cfg.dat \
    $(LOCAL_PATH)/wifi/WCNSS_cfg.dat:$(TARGET_COPY_OUT_SYSTEM)/etc/firmware/wlan/qca_cld/WCNSS_cfg.dat \
    $(LOCAL_PATH)/wifi/WCNSS_qcom_cfg.ini:$(TARGET_COPY_OUT_VENDOR)/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini \
    $(LOCAL_PATH)/wifi/WCNSS_qcom_cfg.ini:$(TARGET_COPY_OUT_SYSTEM)/etc/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini

# only include verity on user builds for LineageOS
ifeq ($(TARGET_BUILD_VARIANT),user)
   PRODUCT_COPY_FILES += $(LOCAL_PATH)/fstab-verity.cityman:root/fstab.cityman

# setup dm-verity configs
PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/platform/soc.0/f9824900.sdhci/by-name/system
#PRODUCT_VENDOR_VERITY_PARTITION := /dev/block/platform/soc.0/f9824900.sdhci/by-name/vendor
$(call inherit-product, build/target/product/verity.mk)
endif

$(call inherit-product-if-exists, hardware/qcom/msm8994/msm8992.mk)
$(call inherit-product-if-exists, vendor/qcom/gpu/msm8994/msm8994-gpu-vendor.mk)

# The prebuilt LG/Qualcomm apps (DCMO, DMConfigUpdate, HiddenMenu,
# LifeTimerService, datastatusnotification) list com.qualcomm.qcrilhook in
# their manifests but not in the build files; Android 13 checks that.
PRODUCT_BROKEN_VERIFY_USES_LIBRARIES := true
