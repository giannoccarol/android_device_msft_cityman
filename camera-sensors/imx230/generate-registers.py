#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate the sensor programs from the pinned public SMIA++ register source."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

REVISION = "6ab0e0e9018d6185b642fa86de8b1c61cbd2701b"
TABLES = "drivers/media/platform/msm/camera_v2/sensor/smia65pp_modes.h"
SHA256 = "744b2e10d70d0da125488df2d9cfe5a503bfaafb79deaffde246c81f479ae317"


def generate(kernel):
    source = subprocess.check_output(["git", "-C", str(kernel), "show", REVISION + ":" + TABLES])
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError("Unexpected upstream register source")
    parts = ["/* SPDX-License-Identifier: GPL-2.0-only */",
             "/* Register programs from the pinned public SMIA++ reference; see ABI.md. */"]
    for original, target in [("smia65pp_idle", "smia_idle"), ("smia65pp_mode4", "smia_mode4")]:
        match = re.search(r"static const struct smia65pp_regval " + original
                          + r"\[\] = \{(.*?)^\};", source.decode(), re.M | re.S)
        if not match:
            raise ValueError("Missing program " + original)
        entries = [(int(a, 16), int(v, 16)) for a, v in re.findall(
            r"\{\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\s*\}", match[1])]
        if not entries or entries.pop() != (0, 0):
            raise ValueError("Missing expected terminator")
        if any(a in (0, 0x0100, 0x0104, 0x0107) for a, _ in entries):
            raise ValueError("Unexpected identity/stream/address control register")
        if target == "smia_mode4":
            entries += [(0x0340, 2550 >> 8), (0x0341, 2550 & 255),
                        (0x0202, 1200 >> 8), (0x0203, 1200 & 255), (0x0204, 0), (0x0205, 0)]
            entries += [(a, 1 if a % 2 == 0 else 0) for a in range(0x020e, 0x0216)]
        parts.append("static struct msm_camera_i2c_reg_array " + target + "[] = {\n"
                     + "".join("    {0x%04x, 0x%02x, 0},\n" % pair for pair in entries) + "};")
    return "\n\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kernel", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = generate(args.kernel)
    target = Path(__file__).with_name("smia_registers.h")
    if args.check:
        if target.read_text() != result:
            parser.exit(1, "Generated register header differs\n")
        print("Pinned register programs match")
    else:
        target.write_text(result)


if __name__ == "__main__":
    main()
