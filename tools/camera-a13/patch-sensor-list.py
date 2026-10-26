#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Patch the private daemon sensor list to probe libmmcamera_vs6955.so.

The daemon list is a compiled string array in the proprietary
libmmcamera2_sensor_modules.so. `imx258` is unused on Cityman; replacing it
with `vs6955` (same length) makes the daemon probe the front plugin without
changing any offsets. Input and output are private and stay outside Git.
"""
import argparse
import hashlib
import sys
from pathlib import Path

INPUT_SHA256 = "2f9949e423c8bb65089fb0264a3ab812274efde578a300b789f6626c2e1d6803"
OLD = b"imx258"
NEW = b"vs6955"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.input.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != INPUT_SHA256:
        parser.exit(1, "Unexpected libmmcamera2_sensor_modules.so: %s\n" % digest)
    if data.count(OLD) != 1:
        parser.exit(1, "Expected exactly one %r entry, found %d\n" % (OLD, data.count(OLD)))
    if args.output.resolve() == args.input.resolve():
        parser.exit(1, "Refusing to overwrite the vendor input\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data.replace(OLD, NEW, 1))
    print("patched %s -> %s" % (args.input, args.output))


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
