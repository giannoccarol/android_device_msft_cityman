# VNDK v29 libraries kept for the vendor blobs

Android 15 and later have no VNDK snapshots. These three AOSP libraries come
unchanged from `prebuilts/vndk/v29` (Android 10, Apache 2.0) and are installed
under the names the cityman vendor blobs expect:

- `arm/libprotobuf-cpp-lite.so` -> `/vendor/lib/libprotobuf-cpp-lite-v29.so`
- `arm64/libprotobuf-cpp-lite.so` -> `/vendor/lib64/libprotobuf-cpp-lite-v29.so`
- `arm64/libcutils.so` -> `/system/lib64/libcutils-v29.so`

Same approach as the OnePlus 2 Android 16 tree (davigamer987/device_oneplus_oneplus2,
`libprotobuf/vndk`).
