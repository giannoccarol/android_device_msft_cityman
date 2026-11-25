# LK and recovery shipped in the ROM zip

The LineageOS Updater downloads a build, writes the install command to `misc` and reboots to
recovery. On a phone set up by the official Android4Lumia950 installer two pieces cannot do that:

- its LK (Android4Lumia950 lk2nd `80dcd67`, SHA-256 `392cabea…`) does not start the recovery
  partition on a reboot to recovery (it also leaves fastboot black);
- its TWRP (`twrp.img`, SHA-256 `02a9c310…` over 35651584 bytes) cannot install Android 16 zips:
  its 3.10 kernel lacks `MADV_WIPEONFORK` and the A16 `update-binary` aborts, and its shell picks the
  A16 vendor libraries and fails.

The zip replaces both, only when it finds exactly those files, at the end of the install
(`releasetools.py`, `OTA_UpdateBootchain`, running `cityman-bootchain.sh` in the recovery). Anything
else (a newer LK, a TWRP the user installed) is left as it is. The zip prints what it did.

| File | What | SHA-256 |
|---|---|---|
| `emmc_appsboot.mbn` | LK of the installer: lk2nd built as lk1st for the 950 XL from the [`cityman` branch of giannoccarol/lk2nd-cityman](https://github.com/giannoccarol/lk2nd-cityman/tree/cityman), `3394d965` (`msm8994-lumiav1-next-3394d965-20261117`), with the panel driver; starts the recovery on a reboot to recovery | `31100f058bb82234fea846177ee8a95bdfb370d11cd45e27b91c2d530f16ce12` |
| `twrp-cityman-a16.img` | Android 16 TWRP (release `twrp-cityman-a16-20261030`): the installer TWRP 3.7.0_9-0 with the A16 kernel and its own linker path | `35f4cd822caec6b5cd8b981b86f6e78169c318bca470d2e1b6bb4c5b319d8233` |
| `cityman-bootchain.sh` | the script the zip runs | |

What the script does:

- LK: when `aboot` (vfat) holds the official LK, copy it to `emmc_appsboot.mbn.official`, write the
  new LK over `emmc_appsboot.mbn` and check it after unmounting; if the check fails, put the official
  LK back. No room for the copy: nothing changes.
- Recovery: when the partition holds no boot image or the old installer TWRP, write the Android 16
  TWRP and check it.

Verified on the developer's phone (25/11/2026, new LK in `aboot`): `adb reboot recovery` starts the
recovery partition.

## Going back

LK: from TWRP mount `aboot` (vfat) and copy `emmc_appsboot.mbn.official` over `emmc_appsboot.mbn`. If
LK did not start at all, the Developer Menu (boot manager entry) or WPInternals mass storage mode give
access to `aboot` from a PC. Recovery: `fastboot flash recovery <image>`.

## Credits

lk2nd ([msm8916-mainline/lk2nd](https://github.com/msm8916-mainline/lk2nd), Stephan Gerhold and
contributors) and the [Android4Lumia950 lk2nd fork](https://github.com/Android4Lumia950/lk2nd) this LK
is built from; the panel driver follows Qualcomm's LK display code for the MSM8994 reference phones
and the panel settings of the Android4Lumia950 MMO kernel. TWRP (TeamWin) and the Android4Lumia950
installer (djokcho, EpicLPer, j0sh1x and the community) for the cityman TWRP build.
