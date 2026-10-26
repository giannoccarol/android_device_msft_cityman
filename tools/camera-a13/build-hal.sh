#!/usr/bin/env bash
# Build camera modules against isolated, committed Android 13 camera sources.
set -euo pipefail
los=$(realpath "${1:?usage: build-hal.sh LOS20 NEW_BUILD_DIR KERNEL_REPO VENDOR_REPO [JOBS]}")
output=$(realpath -m "${2:?supply a new private build directory}")
kernel=$(realpath "${3:?supply the Android 13 camera kernel repository}")
vendor=$(realpath "${4:?supply the Android 13 camera vendor repository}")
jobs=${5:-6}
device=$(git -C "$(dirname -- "$0")" rev-parse --show-toplevel)
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid job count' >&2; exit 1; }
[[ ! -e "$output" ]] || { echo 'Output already exists' >&2; exit 1; }
[[ -d "$los/out/target/product/cityman" ]] || { echo 'Build Cityman once first' >&2; exit 1; }
for repo in "$device" "$kernel" "$vendor"; do
    [[ $(git -C "$repo" branch --show-current) == lineage-20-cityman-camera ]] || {
        echo "Wrong source branch: $repo" >&2; exit 1;
    }
    [[ -z $(git -C "$repo" status --porcelain --untracked-files=normal) ]] || {
        echo "Commit source changes first: $repo" >&2; exit 1;
    }
done
mkdir -p "$output" "$output/device" "$output/kernel" "$output/vendor"
for part in device kernel vendor; do
    repo=${!part}
    revision=$(git -C "$repo" rev-parse HEAD)
    printf '%s %s\n' "$part" "$revision" >> "$output/source-commits.txt"
    git -C "$repo" archive "$revision" | tar -xf - -C "$output/$part"
done
# Fail rather than silently duplicating a large output tree without reflinks.
cp -a --reflink=always "$los/out" "$output/out"
docker run --rm --network none -i \
    --mount "type=bind,src=$los,dst=/src,readonly" \
    --mount "type=bind,src=$output/device,dst=/src/device/msft/cityman,readonly" \
    --mount "type=bind,src=$output/kernel,dst=/src/kernel/mmo/msm8994,readonly" \
    --mount "type=bind,src=$output/vendor,dst=/src/vendor/msft/cityman,readonly" \
    --mount "type=bind,src=$output/out,dst=/src/out" \
    -e JOBS="$jobs" --name "los20-camera-hal-$$" los18-build bash -c '
        set -eo pipefail
        cd /src
        source build/envsetup.sh >/dev/null
        lunch lineage_cityman-userdebug
        m -j"$JOBS" camera.msm8992 libmmcamera_interface libmmjpeg_interface libqomx_core
    ' 2>&1 | tee "$output/build.log"
(
    cd "$output/out/target/product/cityman"
    sha256sum system/lib/hw/camera.msm8992.so system/lib/libmmcamera_interface.so \
        system/lib/libmmjpeg_interface.so system/lib/libqomx_core.so
) > "$output/camera.SHA256SUMS"
