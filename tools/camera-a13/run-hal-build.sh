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
docker run --rm --network none -i \
    --mount "type=bind,src=$los,dst=/src,readonly" \
    --mount "type=bind,src=$output/device,dst=/src/device/msft/cityman,readonly" \
    --mount "type=bind,src=$output/kernel,dst=/src/kernel/mmo/msm8994,readonly" \
    --mount "type=bind,src=$output/vendor,dst=/src/vendor/msft/cityman,readonly" \
    --mount "type=bind,src=$output/out,dst=/src/out" \
    --mount "type=bind,src=$output/ccache,dst=/ccache" \
    -e JOBS="$jobs" --hostname cityman-camera-build \
    --name "los20-camera-hal-$$" los18-build bash -c '
        set -eo pipefail
        cd /src
        source build/envsetup.sh >/dev/null
        lunch lineage_cityman-userdebug
        m -j"$JOBS" camera.msm8992 libmmcamera_interface libmmjpeg_interface libqomx_core
    ' 2>&1 | tee "$output/build.log"
(
    cd "$output/out/target/product/cityman"
    sha256sum system/lib/hw/camera.msm8992.so system/lib/libmmcamera_interface.so \
        vendor/lib/libmmjpeg_interface.so system/lib/libqomx_core.so
) > "$output/camera.SHA256SUMS"
