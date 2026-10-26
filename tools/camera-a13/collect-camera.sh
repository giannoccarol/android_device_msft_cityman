#!/usr/bin/env bash
# Read-only evidence capture; never opens a camera, restarts services or reboots.
set -euo pipefail
serial=${1:?usage: collect-camera.sh ADB_SERIAL NEW_PRIVATE_OUTPUT}
out=$(realpath -m "${2:?supply a new private output directory}")
[[ ! -e "$out" ]] || { echo 'Output already exists' >&2; exit 1; }
adb=(adb -s "$serial")
[[ $(timeout 10 "${adb[@]}" shell getprop ro.product.device | tr -d '\r') == cityman ]] || {
    echo 'The selected device is not Cityman' >&2; exit 1;
}
mkdir -p "$out"
capture() {
    local name=$1
    shift
    if timeout 25 "${adb[@]}" "$@" > "$out/$name" 2> "$out/$name.stderr"; then
        printf '%s OK\n' "$name" >> "$out/capture-status.txt"
    else
        printf '%s FAILED_OR_TIMED_OUT\n' "$name" >> "$out/capture-status.txt"
    fi
}
capture state.txt shell 'uname -a; getprop ro.build.fingerprint; getprop ro.build.date; getprop sys.boot_completed; getprop init.svc.qcamerasvr; getprop init.svc.vendor.camera-provider-2-4; sha256sum /vendor/lib/libmmcamera_imx230.so; cat /proc/uptime'
capture clocks.txt shell 'for p in /sys/kernel/debug/clk/cam_src_clk/rate /sys/kernel/debug/clk/cam0_clk/rate /sys/kernel/debug/clk/cam_src_clk/measure; do if [ -r "$p" ]; then echo "$p"; cat "$p"; fi; done'
capture dmesg.txt shell dmesg
capture media-camera.txt shell dumpsys media.camera
capture logcat.txt logcat -b all -d
(
    cd "$out"
    sha256sum state.txt clocks.txt dmesg.txt media-camera.txt logcat.txt capture-status.txt > SHA256SUMS
)
echo "Saved read-only camera evidence in $out"
