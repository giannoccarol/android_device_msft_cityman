# Copyright (C) 2009 The Android Open Source Project
# Copyright (c) 2011, The Linux Foundation. All rights reserved.
# Copyright (C) 2017-2022 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import common
import hashlib
import re

def FullOTA_InstallBegin(info):
  OTA_RequireLumia(info)
  OTA_RequireExt4Metadata(info)
  return

# The device names accepted above include angler and bullhead, because the
# Lumia TWRPs are built from the Nexus 6P and 5X trees. A real Nexus has no DPP
# (Lumia provisioning) partition: stop there before anything is written.
DPP_BLOCK = "/dev/block/platform/soc.0/f9824900.sdhci/by-name/DPP"

def OTA_RequireLumia(info):
  info.script.AppendExtra('ui_print("Checking for a Lumia...");')
  info.script.AppendExtra(
      '(run_program("/sbin/sh", "-c", "test -e %s") == 0) || abort("%s");'
      % (DPP_BLOCK,
         "This zip is for the Lumia 950 XL and 950: no Lumia DPP partition "
         "was found on this phone. Nothing was written."))
  return

def FullOTA_InstallEnd(info):
  OTA_UpdateFirmware(info)
  OTA_UpdateLK(info)
  return

# The official LK (Android4Lumia950 lk2nd 80dcd67) leaves fastboot black and
# does not start the recovery partition on a reboot to recovery, which the
# Updater needs. When aboot holds exactly that LK, lk/cityman-lk.sh keeps it
# as emmc_appsboot.mbn.official and writes the LK of the zip (lk/README.md).
# Any other LK is left as it is. Runs last: the ROM is already installed.
LK_OFFICIAL_SHA256 = (
    "392cabea0594c0b37f3986527ad95b6b480a554a81b7621587f9c1f11cf2fa6f")
LK_FILES = ("emmc_appsboot.mbn", "cityman-lk.sh")
LK_STATUS = "/tmp/cityman-lk.status"
LK_MESSAGES = (
    ("updated", "Bootloader (LK) updated. The official one is kept in aboot "
                "as emmc_appsboot.mbn.official."),
    ("current", "Bootloader (LK) already up to date."),
    ("other", "Bootloader (LK) is not the official one: left as it is."),
    ("skipped", "Bootloader (LK) left as it is (aboot not usable or full)."),
    ("badfile", "Bootloader (LK) file in the zip is damaged: left as it is."),
    ("restored", "Bootloader (LK) could not be written: the official one is "
                 "back in place."),
    ("broken", "WARNING: the bootloader (LK) could not be written or put "
               "back. Do not reboot. Copy emmc_appsboot.mbn.official over "
               "emmc_appsboot.mbn in the aboot partition (vfat) from the "
               "TWRP shell, then reboot."),
)

def OTA_UpdateLK(info):
  names = info.input_zip.namelist()
  data = {}
  for f in LK_FILES:
    if "RADIO/" + f not in names:
      raise common.ExternalError(
          "RADIO/%s is missing from the target files (device Android.mk)" % f)
    data[f] = info.input_zip.read("RADIO/" + f)
    common.ZipWriteStr(info.output_zip, "install/lk/" + f, data[f])
  lk_sha = hashlib.sha256(data["emmc_appsboot.mbn"]).hexdigest()

  s = info.script
  s.Print("Checking the bootloader (LK)...")
  s.AppendExtra('package_extract_file("install/lk/emmc_appsboot.mbn", '
                '"/tmp/cityman-lk.mbn");')
  s.AppendExtra('package_extract_file("install/lk/cityman-lk.sh", '
                '"/tmp/cityman-lk.sh");')
  s.AppendExtra('run_program("/sbin/sh", "/tmp/cityman-lk.sh", '
                '"/tmp/cityman-lk.mbn", "%s", "%s", "%s");'
                % (LK_OFFICIAL_SHA256, lk_sha, LK_STATUS))
  for word, msg in LK_MESSAGES:
    s.AppendExtra('ifelse(run_program("/sbin/sh", "-c", "grep -qx %s %s") == 0, '
                  'ui_print("%s"));' % (word, LK_STATUS, msg))
  s.AppendExtra('ifelse(run_program("/sbin/sh", "-c", "test -s %s") == 0, "", '
                'ui_print("Bootloader (LK) not checked."));' % LK_STATUS)
  return

# The cache partition is mounted as /metadata, ext4 only. A phone coming from
# another ROM or a custom partition layout can have anything else there; then
# /metadata stays read-only, aconfigd cannot write the flag storage,
# flag-guarded permissions such as android.permission.RANGING disappear and
# system_server crashes in a loop. Formatting it here could destroy data on a
# repartitioned phone, so stop before writing anything and tell the user.
METADATA_BLOCK = "/dev/block/platform/soc.0/f9824900.sdhci/by-name/cache"

def OTA_RequireExt4Metadata(info):
  cmd = 'blkid %s | grep -q \'TYPE="ext4"\'' % METADATA_BLOCK
  info.script.AppendExtra('ui_print("Checking the cache (/metadata) file system...");')
  info.script.AppendExtra(
      '(run_program("/sbin/sh", "-c", "%s") == 0) || abort("%s");'
      % (cmd.replace('\\', '\\\\').replace('"', '\\"'),
         "The cache partition (/metadata) is not ext4. Format it as ext4 "
         "in TWRP (Wipe > Advanced Wipe > Cache > Repair or Change File "
         "System > Change File System > EXT4), then flash this zip again. "
         "Nothing was written."))
  return

def IncrementalOTA_InstallEnd(info):
  OTA_UpdateFirmware(info)
  return

def IncrementalOTA_Assertions(info):
  AddModemAssertion(info, info.target_zip)
  return

def AddImage(info, basename, dest):
  path = "IMAGES/" + basename
  if path not in info.input_zip.namelist():
    return

  data = info.input_zip.read(path)
  common.ZipWriteStr(info.output_zip, basename, data)
  info.script.Print("Patching {} image unconditionally...".format(dest.split('/')[-1]))
  info.script.AppendExtra('package_extract_file("%s", "%s");' % (basename, dest))

def OTA_UpdateFirmware(info):
  modem_file = "INSTALL/firmware-update/NON-HLOS.bin"
  if modem_file not in info.input_zip.namelist():
    return

  info.script.AppendExtra('ui_print("Patching modem image unconditionally...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/NON-HLOS.bin", "/dev/block/bootdevice/by-name/modem");')
  return