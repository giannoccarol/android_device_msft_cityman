#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Make the camera daemon probe libmmcamera_ck71ir.so after the front camera.

The daemon's sensor list is a pointer array in the proprietary
libmmcamera2_sensor_modules.so (file offset 0x37980 in the Cityman copy,
nine entries ending ..., "s5k3m2xm", "ov4688", "vs6955"). The daemon probes
the names in that order and the kernel registers the camera nodes in probe
order, so the iris must come after vs6955 or it would take HAL id 1.
Both strings are six characters long: "ov4688" (unused here) becomes
"vs6955" and the last entry becomes "ck71ir". Input and output are private
and stay outside Git.
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

INPUT_SHA256 = "2d14928f825d21dc3c0f1d7fd4ad3610cdfb56de34b0122eca213da1064bec1b"
ARRAY_OFFSET = 0x37980
OLD = b"s5k3m2xm\0ov4688\0vs6955\0"
NEW = b"s5k3m2xm\0vs6955\0ck71ir\0"


def segments(data):
    phoff = struct.unpack_from("<I", data, 0x1c)[0]
    phnum = struct.unpack_from("<H", data, 0x2c)[0]
    out = []
    for i in range(phnum):
        kind, offset, vaddr, _, size, _ = struct.unpack_from("<IIIIII", data, phoff + 32 * i)
        if kind == 1:
            out.append((offset, vaddr, size))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = bytearray(args.input.read_bytes())
    digest = hashlib.sha256(data).hexdigest()
    if digest != INPUT_SHA256:
        parser.exit(1, "Unexpected libmmcamera2_sensor_modules.so: %s\n" % digest)
    if data.count(OLD) != 1:
        parser.exit(1, "Sensor name block not found exactly once\n")
    where = data.find(OLD)
    # The last two array slots must point at "ov4688" and "vs6955".
    base = [(o, v, s) for o, v, s in segments(data) if o <= where < o + s][0]
    ov = where + 9 - base[0] + base[1]
    vs = where + 16 - base[0] + base[1]
    slots = struct.unpack_from("<9I", data, ARRAY_OFFSET)
    if slots[7] != ov or slots[8] != vs:
        parser.exit(1, "Sensor pointer array does not end with ov4688, vs6955\n")
    if args.output.resolve() == args.input.resolve():
        parser.exit(1, "Refusing to overwrite the vendor input\n")
    data[where:where + len(OLD)] = NEW
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(bytes(data))
    print("patched %s -> %s" % (args.input, args.output))


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
