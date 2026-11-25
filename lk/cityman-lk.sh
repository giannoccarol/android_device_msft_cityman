#!/sbin/sh
# Run by the ROM zip in the recovery (releasetools.py, OTA_UpdateLK).
# Replaces the official Lumia 950 XL LK in aboot with the LK of the zip and
# keeps the official one next to it. Any other LK is left as it is.
#
# Usage: cityman-lk.sh <new LK file> <official sha256> <new sha256> <status file>
# The status file gets one word, read by the updater script:
#   updated      the new LK is in place, the official one kept as .official
#   current      aboot already holds the new LK
#   other        aboot holds an LK this zip does not know: left as it is
#   skipped      aboot missing, not mountable, no LK file or no room: left as it is
#   badfile      the LK in the zip is damaged: left as it is
#   restored     writing the new LK failed, the official one is back
#   broken       writing failed and the official LK could not be put back

new=$1
official_sha=$2
new_sha=$3
status=$4
dev=${CITYMAN_ABOOT_DEV:-/dev/block/platform/soc.0/f9824900.sdhci/by-name/aboot}
mnt=/tmp/cityman-aboot
lk=$mnt/emmc_appsboot.mbn
bak=$mnt/emmc_appsboot.mbn.official

say() { echo "$1" > "$status"; echo "cityman-lk: $1${2:+ ($2)}"; }
sha() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }
unmount() { sync; umount "$mnt" 2>/dev/null; }

[ "$(sha "$new")" = "$new_sha" ] || { say badfile; exit 0; }
[ -b "$dev" ] || { say skipped "no aboot partition"; exit 0; }

mkdir -p "$mnt"
umount "$mnt" 2>/dev/null
mount -t vfat "$dev" "$mnt" 2>/dev/null || { say skipped "aboot not mountable"; exit 0; }

cur=none
[ -f "$lk" ] && cur=$(sha "$lk")
if [ "$cur" = "$new_sha" ]; then unmount; say current; exit 0; fi
if [ "$cur" != "$official_sha" ]; then unmount; say other "$cur"; exit 0; fi

# Copy of the official LK first; without room for it nothing changes.
if [ "$(sha "$bak")" != "$official_sha" ]; then
	if ! cp "$lk" "$bak" || [ "$(sha "$bak")" != "$official_sha" ]; then
		rm -f "$bak"
		unmount
		say skipped "no room for the copy of the official LK"
		exit 0
	fi
fi

# Overwrite in place: the directory entry of emmc_appsboot.mbn stays the same.
cp "$new" "$lk"
unmount

# Check what is on the partition, not what is in the page cache.
mount -t vfat -o ro "$dev" "$mnt" 2>/dev/null && got=$(sha "$lk")
umount "$mnt" 2>/dev/null
if [ "$got" = "$new_sha" ]; then say updated; exit 0; fi

mount -t vfat "$dev" "$mnt" 2>/dev/null && cp "$bak" "$lk"
unmount
mount -t vfat -o ro "$dev" "$mnt" 2>/dev/null && got=$(sha "$lk")
umount "$mnt" 2>/dev/null
if [ "$got" = "$official_sha" ]; then say restored; else say broken "$got"; fi
exit 0
