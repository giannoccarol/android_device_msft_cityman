#!/vendor/bin/sh
#
# Load the WLAN driver, first making sure the per-device MAC files exist.
#
# qcacld reads its MAC addresses from /persist/wlan_mac.bin (through the
# wlan/qca_cld/wlan_mac.bin link) and, with g_use_otpmac=0, refuses to start
# without it. Bluetooth reads /persist/bdaddr.txt. The installer's
# provision.sh writes both from the DPP partition, but /persist is never
# touched by the ROM: a phone installed without that step had no Wi-Fi.
# Write them here the same way when they are missing, from the DPP mounted
# read-only by fstab; with no usable DPP, use a random locally administered
# address kept in /persist so it stays the same across boots.
#
# usage: init.cityman.wlan.sh [<persist dir> [<DPP QCOM dir>] noload]
#        (arguments only for tests; init runs it without any)

PERSIST=${1:-/persist}
DPP=${2:-/mnt/vendor/dpp/QCOM}
WMAC=$PERSIST/wlan_mac.bin
BDADDR=$PERSIST/bdaddr.txt

say() {
	{ echo "cityman-wlan: $*" > /dev/kmsg; } 2>/dev/null || echo "cityman-wlan: $*"
}

# <file> <offset> <count>: bytes as uppercase hex, no separators
hexbytes() {
	od -An -tx1 -j "$2" -N "$3" "$1" 2>/dev/null | tr -d ' \n' | tr a-f A-F
}

# 12 hex digits, not all zero or all ones
valid_mac() {
	case "$1" in
	000000000000|FFFFFFFFFFFF) return 1 ;;
	esac
	[ ${#1} -eq 12 ] && [ -z "$(echo "$1" | tr -d 0-9A-F)" ]
}

# <mac> <n>: same address with n added to the last byte
mac_add() {
	printf '%s%02X' "${1%??}" $(( (16#${1#??????????} + $2) & 255 ))
}

# random locally administered unicast address
random_mac() {
	echo "02$(hexbytes /dev/urandom 0 5)"
}

# write <file> from stdin, atomically
put() {
	cat > "$1.tmp" && chmod 0644 "$1.tmp" && mv -f "$1.tmp" "$1" && sync
	restorecon "$1" 2>/dev/null
}

if [ ! -s "$WMAC" ]; then
	m0=$(hexbytes "$DPP/WLAN.PROVISION" 3 6)
	m1=$(hexbytes "$DPP/WLAN.PROVISION" 9 6)
	m2=$(hexbytes "$DPP/WLAN.PROVISION" 15 6)
	m3=$(hexbytes "$DPP/WLAN.PROVISION" 21 6)
	if valid_mac "$m0" && valid_mac "$m1" && valid_mac "$m2" && valid_mac "$m3"; then
		src=DPP
	else
		m0=$(random_mac)
		m1=$(mac_add "$m0" 1)
		m2=$(mac_add "$m0" 2)
		m3=$(mac_add "$m0" 3)
		src=random
	fi
	printf 'Intf0MacAddress=%s\nIntf1MacAddress=%s\nIntf2MacAddress=%s\nIntf3MacAddress=%s\n' \
		"$m0" "$m1" "$m2" "$m3" | put "$WMAC" &&
		say "created $WMAC from $src" || say "could not write $WMAC"
fi

if [ ! -s "$BDADDR" ]; then
	b=$(hexbytes "$DPP/BT.PROVISION" 2 6)
	if valid_mac "$b"; then
		src=DPP
	else
		b=$(random_mac)
		src=random
	fi
	echo "$b" | sed 's/\(..\)/\1:/g; s/:$//' | put "$BDADDR" &&
		say "created $BDADDR from $src" || say "could not write $BDADDR"
fi

# The driver is built in and starts when fwpath is written (the write
# blocks while it initializes).
case " $* " in
*" noload "*) ;;
*) echo sta > /sys/module/wlan/parameters/fwpath ;;
esac
