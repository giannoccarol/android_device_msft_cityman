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
exec "$device/tools/camera-a13/run-hal-build.sh" "$los" "$output" "$jobs"
