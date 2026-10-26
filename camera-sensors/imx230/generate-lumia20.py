#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate the private Lumia 20 MP IMX230 mode tables from the stock CDCC.

The init is the public smia_idle table already in smia_registers.h; this
generator only extracts the Cityman mode deltas. The DCC input is
proprietary and stays outside Git.
"""
import argparse
import hashlib
import sys
from pathlib import Path

DCC_SHA256 = "6fcbdf9d37893224bf864bb16979f631a919cfad97a809788e0da8a161a4d988"
MODES = {
    "lumia20_mode0": (0x09D3, 73),  # 5344x3744, 20 MP 4:3
    "lumia20_mode1": (0x0AB1, 73),  # 5344x3008, 16 MP 16:9
}


def parse_run(data, start, expected):
    entries = []
    offset = start
    while offset + 3 <= len(data):
        addr = (data[offset] << 8) | data[offset + 1]
        value = data[offset + 2]
        if addr == 0 and value == 0:
            break
        entries.append((addr, value, offset))
        offset += 3
        if len(entries) > expected:
            break
    if len(entries) != expected:
        raise ValueError("Expected %d writes at 0x%x, got %d" % (expected, start, len(entries)))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dcc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.dcc.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != DCC_SHA256:
        parser.exit(1, "Unexpected Lumia DCC: %s\n" % digest)
    lines = [
        "/* SPDX-License-Identifier: GPL-2.0-only */",
        "/* Private vendor register data: diagnostic input, do not commit. */",
        "/* Input SHA-256: " + DCC_SHA256 + " */",
        "#ifndef LUMIA20_REGISTERS_GENERATED_H",
        "#define LUMIA20_REGISTERS_GENERATED_H",
        "",
    ]
    for name, (offset, expected) in MODES.items():
        entries = parse_run(data, offset, expected)
        lines.append("/* %s: %d writes at 0x%05x */" % (name, expected, offset))
        lines.append("static struct msm_camera_i2c_reg_array %s[] = {" % name)
        lines += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */" % (a, v, o) for a, v, o in entries]
        lines += ["};", ""]
    lines += ["#endif /* LUMIA20_REGISTERS_GENERATED_H */", ""]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text("\n".join(lines))
    temporary.replace(args.output)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
