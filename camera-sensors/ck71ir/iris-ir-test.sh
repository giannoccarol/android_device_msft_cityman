#!/system/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Light one PMI8994 flash channel in torch mode for a bounded time, to find
# which channel drives the iris IR LED. Run as root on the phone:
#   iris-ir-test.sh <0|1> [mA, default 25, max 100] [seconds, default 5, max 20]
# Look at the LED with another phone's camera (most front cameras see
# 850 nm), not with the eye, and keep it at arm's length.
ch=${1:?usage: iris-ir-test.sh <0|1> [mA] [seconds]}
ma=${2:-25}
sec=${3:-5}
case "$ch" in 0|1) ;; *) echo "channel must be 0 or 1" >&2; exit 2 ;; esac
[ "$ma" -ge 0 ] 2>/dev/null && [ "$ma" -le 100 ] || { echo "mA must be 0..100" >&2; exit 2; }
[ "$sec" -ge 1 ] 2>/dev/null && [ "$sec" -le 20 ] || { echo "seconds must be 1..20" >&2; exit 2; }
led=/sys/class/leds/led:torch_$ch
[ -w "$led/brightness" ] || { echo "$led not writable" >&2; exit 3; }
trap 'echo 0 > "$led/brightness"' EXIT INT TERM
echo "torch_$ch: $ma mA for $sec s (max_brightness $(cat "$led/max_brightness"))"
echo "$ma" > "$led/brightness"
sleep "$sec"
