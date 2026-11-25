#!/sbin/sh
# Run by the ROM zip in the recovery (releasetools.py, OTA_UpdateBootchain).
# Brings the LK in aboot and the recovery partition to what the Updater needs,
# only where the phone still has the files of the official installer.
#
#   cityman-bootchain.sh lk <new LK> <official sha256> <new sha256> <status file>
#     Official LK in aboot: keep it as emmc_appsboot.mbn.official, write the
#     new one over emmc_appsboot.mbn. Any other LK is left as it is.
#   cityman-bootchain.sh recovery <new image> <new sha256> <old sha256> <old size> <status file>
#     Recovery partition empty or holding the old TWRP of the official
#     installer (sha256 of its first <old size> bytes): write the new image.
#     Any other recovery is left as it is.
#
# The status file gets one word, read by the updater script:
#   updated   written and checked
#   current   already there
#   other     a file or image this zip does not know: left as it is
#   skipped   partition missing, not mountable, too small or full: left as it is
#   badfile   the file in the zip is damaged: left as it is
#   restored  (lk) writing failed, the official LK is back
#   broken    (lk) writing failed and the official LK could not be put back
#   failed    (recovery) written but the check failed

sha() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }
say() { echo "$1" > "$status"; echo "cityman-bootchain $what: $1${2:+ ($2)}"; }

update_lk() {
	new=$1 official_sha=$2 new_sha=$3 status=$4
	dev=${CITYMAN_ABOOT_DEV:-/dev/block/platform/soc.0/f9824900.sdhci/by-name/aboot}
	mnt=/tmp/cityman-aboot
	lk=$mnt/emmc_appsboot.mbn
	bak=$mnt/emmc_appsboot.mbn.official
	unmount() { sync; umount "$mnt" 2>/dev/null; }

	[ "$(sha "$new")" = "$new_sha" ] || { say badfile; return; }
	[ -b "$dev" ] || { say skipped "no aboot partition"; return; }

	mkdir -p "$mnt"
	umount "$mnt" 2>/dev/null
	mount -t vfat "$dev" "$mnt" 2>/dev/null || { say skipped "aboot not mountable"; return; }

	cur=none
	[ -f "$lk" ] && cur=$(sha "$lk")
	if [ "$cur" = "$new_sha" ]; then unmount; say current; return; fi
	if [ "$cur" != "$official_sha" ]; then unmount; say other "$cur"; return; fi

	# Copy of the official LK first; without room for it nothing changes.
	if [ "$(sha "$bak")" != "$official_sha" ]; then
		if ! cp "$lk" "$bak" || [ "$(sha "$bak")" != "$official_sha" ]; then
			rm -f "$bak"
			unmount
			say skipped "no room for the copy of the official LK"
			return
		fi
	fi

	# Overwrite in place: the directory entry of emmc_appsboot.mbn stays.
	cp "$new" "$lk"
	unmount

	# Check what is on the partition, not what is in the page cache.
	got=
	mount -t vfat -o ro "$dev" "$mnt" 2>/dev/null && got=$(sha "$lk")
	umount "$mnt" 2>/dev/null
	if [ "$got" = "$new_sha" ]; then say updated; return; fi

	mount -t vfat "$dev" "$mnt" 2>/dev/null && cp "$bak" "$lk"
	unmount
	got=
	mount -t vfat -o ro "$dev" "$mnt" 2>/dev/null && got=$(sha "$lk")
	umount "$mnt" 2>/dev/null
	if [ "$got" = "$official_sha" ]; then say restored; else say broken "$got"; fi
}

update_recovery() {
	new=$1 new_sha=$2 old_sha=$3 old_size=$4 status=$5
	dev=${CITYMAN_RECOVERY_DEV:-/dev/block/platform/soc.0/f9824900.sdhci/by-name/recovery}

	[ "$(sha "$new")" = "$new_sha" ] || { say badfile; return; }
	[ -b "$dev" ] || { say skipped "no recovery partition"; return; }
	# No stat -c: the busybox stat of TWRP does not have it.
	size=$(wc -c < "$new" | tr -d ' ')
	part=$(blockdev --getsize64 "$dev" 2>/dev/null)
	if [ -z "$part" ]; then
		blk=$(basename "$(readlink -f "$dev")")
		[ -r "/sys/class/block/$blk/size" ] &&
			part=$(( $(cat "/sys/class/block/$blk/size") * 512 ))
	fi
	[ -n "$size" ] && [ -n "$part" ] && [ "$part" -ge "$size" ] ||
		{ say skipped "recovery partition too small or unknown size (${part:-?} < ${size:-?})"; return; }

	cur=$(head -c "$size" "$dev" | sha256sum | cut -d' ' -f1)
	if [ "$cur" = "$new_sha" ]; then say current; return; fi
	if [ "$(head -c 8 "$dev")" = "ANDROID!" ]; then
		old=$(head -c "$old_size" "$dev" | sha256sum | cut -d' ' -f1)
		[ "$old" = "$old_sha" ] || { say other; return; }
	fi

	dd if="$new" of="$dev" bs=1048576 2>/dev/null
	sync
	blockdev --flushbufs "$dev" 2>/dev/null
	echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
	got=$(head -c "$size" "$dev" | sha256sum | cut -d' ' -f1)
	if [ "$got" = "$new_sha" ]; then say updated; else say failed "$got"; fi
}

what=$1
shift
case "$what" in
lk) update_lk "$@" ;;
recovery) update_recovery "$@" ;;
*) echo "cityman-bootchain: unknown command $what" ;;
esac
exit 0
