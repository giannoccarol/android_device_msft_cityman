#!/usr/bin/env bash
# Run or resume a camera-module build using the already archived source inputs.
set -euo pipefail
los=$(realpath "${1:?usage: run-hal-build.sh LOS20 EXISTING_BUILD_DIR [JOBS]}")
output=$(realpath "${2:?supply the existing private build directory}")
jobs=${3:-6}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid job count' >&2; exit 1; }
[[ -f "$output/source-commits.txt" ]] || { echo 'Missing source manifest' >&2; exit 1; }
for part in device kernel vendor out; do
    [[ -d "$output/$part" ]] || { echo "Missing input: $part" >&2; exit 1; }
done
mkdir -p "$output/ccache"
profile=smia9
[[ ! -f "$output/sensor-profile.txt" ]] || profile=$(cat "$output/sensor-profile.txt")
profile_mounts=()
case "$profile" in
    smia9) ;;
    nubia24)
        [[ -f "$output/private-inputs/libmmcamera_imx230.so" ]] || { echo 'Missing private sensor input' >&2; exit 1; }
        profile_mounts+=(--mount "type=bind,src=$output/private-inputs,dst=/camera-inputs,readonly")
        ;;
    *) echo "Unsupported saved profile: $profile" >&2; exit 1 ;;
esac
docker run --rm --network none -i \
    --mount "type=bind,src=$los,dst=/src,readonly" \
    --mount "type=bind,src=$output/device,dst=/src/device/msft/cityman,readonly" \
    --mount "type=bind,src=$output/kernel,dst=/src/kernel/mmo/msm8994,readonly" \
    --mount "type=bind,src=$output/vendor,dst=/src/vendor/msft/cityman,readonly" \
    --mount "type=bind,src=$output/out,dst=/src/out" \
    --mount "type=bind,src=$output/ccache,dst=/ccache" \
    "${profile_mounts[@]}" -e CAMERA_SENSOR_PROFILE="$profile" \
    -e JOBS="$jobs" --hostname cityman-camera-build \
    --name "los20-camera-hal-$$" los18-build bash -c '
        set -eo pipefail
        cd /src
        source build/envsetup.sh >/dev/null
        lunch lineage_cityman-userdebug
        modules=(camera.msm8992 libmmcamera_interface libmmjpeg_interface libqomx_core)
        if [[ -f device/msft/cityman/camera-sensors/imx230/Android.mk ]]; then
            modules+=(libmmcamera_imx230)
        fi
        profile_args=("CITYMAN_CAMERA_SENSOR_PROFILE=$CAMERA_SENSOR_PROFILE")
        if [[ "$CAMERA_SENSOR_PROFILE" == nubia24 ]]; then
            profile_args+=(CITYMAN_CAMERA_NUBIA_SENSOR_BLOB=/camera-inputs/libmmcamera_imx230.so)
        fi
        m -j"$JOBS" "${profile_args[@]}" "${modules[@]}"
    ' 2>&1 | tee "$output/build.log"
(
    cd "$output/out/target/product/cityman"
    sha256sum system/lib/hw/camera.msm8992.so system/lib/libmmcamera_interface.so \
        vendor/lib/libmmjpeg_interface.so system/lib/libqomx_core.so
    if [[ -f "$output/device/camera-sensors/imx230/Android.mk" ]]; then
        sha256sum vendor/lib/libmmcamera_imx230.so
    fi
) > "$output/camera.SHA256SUMS"
