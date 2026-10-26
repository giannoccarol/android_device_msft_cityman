#!/usr/bin/env bash
# Run from a configured Android 13 source root/container. No source reset or flash.
set -eo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
profile=${CITYMAN_CAMERA_SENSOR_PROFILE:-nubia24}
jobs=${JOBS:-6}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid job count' >&2; exit 1; }
[[ -f build/envsetup.sh ]] || { echo 'Run from the Android 13 source root' >&2; exit 1; }
patch="$here/patches/0001-frameworks-av-camera-parcels-android13.diff"
if ! git -C frameworks/av apply -p3 --reverse --check "$patch"; then
    echo 'The required Android 13 camera Parcel patch is missing or differs.' >&2
    echo "Review/apply $patch in frameworks/av with git apply -p3 before building." >&2
    exit 1
fi
args=(CITYMAN_CAMERA_SMIA_EXPERIMENTAL=true "CITYMAN_CAMERA_SENSOR_PROFILE=$profile")
case "$profile" in
    smia9) ;;
    nubia24)
        blob=${CITYMAN_CAMERA_NUBIA_SENSOR_BLOB:?supply the pinned private Nubia sensor ELF}
        # A stable relative build path also accepts host input paths with spaces.
        inputs=out/cityman-camera-inputs
        mkdir -p "$inputs"
        python3 "$here/../../camera-sensors/imx230/generate-nubia24.py" "$blob" \
            "$inputs/nubia24_registers.generated.h"
        install -m 600 "$blob" "$inputs/libmmcamera_imx230.so"
        sha256sum "$inputs/libmmcamera_imx230.so" > "$inputs/SHA256SUMS"
        args+=("CITYMAN_CAMERA_NUBIA_SENSOR_BLOB=$inputs/libmmcamera_imx230.so")
        ;;
    *) echo "Unsupported sensor profile: $profile" >&2; exit 1 ;;
esac
source build/envsetup.sh >/dev/null
lunch lineage_cityman-userdebug
printf '%s\n' "$profile" > out/cityman-camera-profile.txt
m -j"$jobs" "${args[@]}" bacon
