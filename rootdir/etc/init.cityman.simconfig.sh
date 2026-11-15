#!/vendor/bin/sh
#
# Dual SIM: the Lumia 950 XL Dual SIM (RM-1116) and the Lumia 950 Dual SIM
# (RM-1118) have two SIM slots on one modem (dual SIM dual standby). The model
# is the TYPE line of Nokia/product.dat in the DPP partition, written at the
# factory. Runs at post-fs-data, before rild, the radio HALs and telephony
# read persist.radio.multisim.config.
#
# Single SIM models keep the property empty (one slot), as before.

dpp=/dev/block/bootdevice/by-name/DPP
model=$(dd if=$dpp bs=4096 count=2048 2>/dev/null |
	grep -a -o -m 1 'TYPE: *RM-[0-9]*' | grep -o 'RM-[0-9]*')

case "$model" in
RM-1116|RM-1118)
	setprop persist.radio.multisim.config dsds
	;;
*)
	# A value left over from a dual SIM phone or set by hand.
	if [ "$(getprop persist.radio.multisim.config)" = dsds ]; then
		setprop persist.radio.multisim.config ""
	fi
	;;
esac

log -t cityman-simconfig "model ${model:-unknown}, multisim '$(getprop persist.radio.multisim.config)'"
