#!/usr/bin/env bash
# Build camera modules against isolated, committed Android 13 camera sources.
set -euo pipefail
los=$(realpath "${1:?usage: build-hal.sh LOS20 NEW_BUILD_DIR KERNEL_REPO VENDOR_REPO [JOBS]}")
output=$(realpath -m "${2:?supply a new private build directory}")
kernel=$(realpath "${3:?supply the Android 13 camera kernel repository}")
vendor=$(realpath "${4:?supply the Android 13 camera vendor repository}")
jobs=${5:-6}
profile=${CITYMAN_CAMERA_SENSOR_PROFILE:-smia9}
device=$(git -C "$(dirname -- "$0")" rev-parse --show-toplevel)
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid job count' >&2; exit 1; }
[[ ! -e "$output" ]] || { echo 'Output already exists' >&2; exit 1; }
[[ -d "$los/out/target/product/cityman" ]] || { echo 'Build Cityman once first' >&2; exit 1; }
case "$profile" in
    smia9) ;;
    nubia24) blob=$(realpath "${CITYMAN_CAMERA_NUBIA_SENSOR_BLOB:?supply the pinned private sensor ELF}") ;;
    *) echo "Unsupported profile: $profile" >&2; exit 1 ;;
esac
for repo in "$device" "$kernel" "$vendor"; do
    [[ $(git -C "$repo" branch --show-current) == lineage-20-cityman-camera ]] || {
        echo "Wrong source branch: $repo" >&2; exit 1;
    }
    [[ -z $(git -C "$repo" status --porcelain --untracked-files=normal) ]] || {
        echo "Commit source changes first: $repo" >&2; exit 1;
    }
done
mkdir -p "$output" "$output/device" "$output/kernel" "$output/vendor"
printf '%s\n' "$profile" > "$output/sensor-profile.txt"
if [[ "$profile" == nubia24 ]]; then
    mkdir -m 700 "$output/private-inputs"
    install -m 600 "$blob" "$output/private-inputs/libmmcamera_imx230.so"
    python3 "$device/camera-sensors/imx230/generate-nubia24.py" \
        "$output/private-inputs/libmmcamera_imx230.so" "$output/private-inputs/nubia24_registers.generated.h"
    sha256sum "$output/private-inputs/libmmcamera_imx230.so" > "$output/private-inputs/SHA256SUMS"
fi
for part in device kernel vendor; do
    repo=${!part}
    revision=$(git -C "$repo" rev-parse HEAD)
    printf '%s %s\n' "$part" "$revision" >> "$output/source-commits.txt"
    git -C "$repo" archive "$revision" | tar -xf - -C "$output/$part"
done
# Fail rather than silently duplicating a large output tree without reflinks.
cp -a --reflink=always "$los/out" "$output/out"
exec "$device/tools/camera-a13/run-hal-build.sh" "$los" "$output" "$jobs"
