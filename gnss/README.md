# Cityman GNSS 1.0 HIDL

AOSP `android.hardware.gnss@1.0-impl` copies NMEA with
`hidl_string::setToExternal(nmea, length)`. CAF
`loc_eng_nmea_put_checksum` on this MPSS returns a length that omits
the leading `$`, so the CHECK `data[length] == '\0'` SIGABRTs the HAL
on the first SV/NMEA sentence.

`1.0/default/` is the Lineage 18.1 impl with `nmeaCb` building the HIDL
string from the NUL-terminated C buffer instead. Organic Maps uses
LocationManager, not NMEA; this only keeps the HAL alive.

Do not vendor a patched `libloc_eng.so`. That was a Magisk test overlay.
