# LK shipped in the ROM zip

`emmc_appsboot.mbn` is the LK of the installer (`lk\emmc_appsboot.mbn` of
[installer-cityman-a16](https://github.com/giannoccarol/installer-cityman-a16)): lk2nd built as
lk1st for the Lumia 950 XL from the
[`cityman` branch of giannoccarol/lk2nd-cityman](https://github.com/giannoccarol/lk2nd-cityman/tree/cityman),
commit `3394d965` (`msm8994-lumiav1-next-3394d965-20261117`), with
`tools/lk2nd/build-lk1st-cityman.sh` of the project.

SHA-256 `31100f058bb82234fea846177ee8a95bdfb370d11cd45e27b91c2d530f16ce12`.

## What the zip does with it

`releasetools.py` (`OTA_UpdateLK`) adds it to the zip and, at the end of the install, runs
`lk/cityman-lk.sh` in the recovery:

- `aboot` holds the official LK (Android4Lumia950 lk2nd `80dcd67`, SHA-256 `392cabea…`): it keeps a
  copy as `emmc_appsboot.mbn.official` in `aboot` and writes this one over `emmc_appsboot.mbn`, then
  checks it after unmounting. If the check fails it puts the official LK back.
- `aboot` already holds this LK, or any LK the zip does not know: left as it is.
- `aboot` missing or not mountable, no room for the copy: left as it is.

The official LK leaves the screen black in fastboot and does not start the recovery partition on a
reboot to recovery, which the Updater needs to install an update.

## Going back

From TWRP mount `aboot` (vfat) and copy `emmc_appsboot.mbn.official` over `emmc_appsboot.mbn`. If LK
did not start at all, the Developer Menu (boot manager entry) or WPInternals mass storage mode give
access to `aboot` from a PC.

## Credits

lk2nd ([msm8916-mainline/lk2nd](https://github.com/msm8916-mainline/lk2nd), Stephan Gerhold and
contributors) and the [Android4Lumia950 lk2nd fork](https://github.com/Android4Lumia950/lk2nd) this LK
is built from; the panel driver follows Qualcomm's LK display code for the MSM8994 reference phones
and the panel settings of the Android4Lumia950 MMO kernel.
