#!/usr/bin/env python3
"""Write the build list of the LineageOS 23 Updater from a released ROM zip.

product.prop points lineage.updater.uri at

    https://raw.githubusercontent.com/giannoccarol/android_device_msft_cityman/ota/cityman.json

that is cityman.json in the "ota" branch of the device repository. The
LineageOS 23 Updater fetches the list without following redirects, so the
releases/latest/download/... address of GitHub (a 302) cannot be used, and a
writable property holds at most 91 bytes.

Every release:

    tools/updater-json.py lineage-23.2-20261126-UNOFFICIAL-cityman.zip \\
        --tag lineage-23.2-20261126-cityman -o cityman.json

then commit cityman.json to the ota branch (README-cityman.md has the steps)
once the zip is attached to the release under the same name.

The list uses the API v2 format (org.lineageos.updater NetworkUpdate). The
Updater offers a build when "datetime" is newer than ro.build.date.utc of the
running system, "type" equals ro.lineage.releasetype (case ignored) and
"os_sdk_level" is not lower than the running Android (missing counts as 0,
which hides the build). datetime, os_sdk_level and os_patch_level come from
the metadata inside the zip.
"""

import argparse
import hashlib
import json
import os
import re
import sys
import zipfile

REPO = "giannoccarol/android_device_msft_cityman"
NAME_RE = re.compile(r"^lineage-(?P<version>[0-9.]+)-(?P<date>[0-9]{8})-"
                     r"(?P<type>[A-Za-z]+)-(?P<device>[A-Za-z0-9_]+)\.zip$")


def read_metadata(zf):
    text = zf.read("META-INF/com/android/metadata").decode()
    meta = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if sep:
            meta[key.strip()] = value.strip()
    return meta


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("zip", help="ROM zip exactly as it is attached to the release")
    ap.add_argument("--tag", required=True, help="release tag")
    ap.add_argument("--repo", default=REPO, help="GitHub owner/name that holds the release")
    ap.add_argument("-o", "--out", default="cityman.json", help="output file")
    args = ap.parse_args()

    filename = os.path.basename(args.zip)
    m = NAME_RE.match(filename)
    if not m:
        sys.exit(f"{filename}: not a lineage-<version>-<date>-<type>-<device>.zip name; "
                 "the Updater shows the name, keep the one the build made")

    with zipfile.ZipFile(args.zip) as zf:
        meta = read_metadata(zf)

    if m["device"] not in meta.get("pre-device", "").split(","):
        sys.exit(f"{filename}: the zip is for {meta.get('pre-device')}, not {m['device']}")
    try:
        timestamp = int(meta["post-timestamp"])
        sdk = int(meta["post-sdk-level"])
    except (KeyError, ValueError):
        sys.exit(f"{filename}: post-timestamp or post-sdk-level missing in "
                 "META-INF/com/android/metadata")

    entry = {
        "datetime": timestamp,
        "type": m["type"],
        "version": m["version"],
        "files": [{
            "filename": filename,
            "os_patch_level": meta.get("post-security-patch-level", ""),
            "os_sdk_level": sdk,
            "sha256": sha256_of(args.zip),
            "size": os.path.getsize(args.zip),
            "url": f"https://github.com/{args.repo}/releases/download/{args.tag}/{filename}",
        }],
    }
    with open(args.out, "w") as f:
        json.dump([entry], f, indent=2)
        f.write("\n")
    print(f"{args.out}: {filename}, {entry['files'][0]['size']} bytes, datetime {timestamp}, "
          f"sdk {sdk}, sha256 {entry['files'][0]['sha256'][:16]}...")


if __name__ == "__main__":
    main()
