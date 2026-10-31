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
import re

def FullOTA_InstallBegin(info):
  OTA_RequireExt4Metadata(info)
  return

def FullOTA_InstallEnd(info):
  OTA_UpdateFirmware(info)
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