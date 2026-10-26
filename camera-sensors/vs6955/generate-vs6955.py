#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate the private VS6955 front-camera register header from the stock
Lumia CDCC 0A214000.dcc. The DCC is proprietary input and stays outside Git.
"""
import argparse
import hashlib
import sys
from pathlib import Path

DCC_SHA256 = "16cc9406ad86f149115181596bd62fb0eb7231ede319df15bfe53575a26be10c"
INIT_START = 0x385
INIT_END = 0xD00


def parse_run(data, start, end):
    out = []
    i = start
    while i + 3 <= end:
        addr = (data[i] << 8) | data[i + 1]
        value = data[i + 2]
        out.append((addr, value, i))
        i += 3
        if data[i:i + 3] == b"\x00\x00\x00":
            break
    return out


def parse_modes(data, start=0xD00, end=0x1200):
    starts = []
    i = start
    while i < end:
        j = data.find(b"\x01\x11", i)
        if j < 0:
            break
        if data[j + 3:j + 5] == b"\x01\x12":
            starts.append(j)
            i = j + 1
        else:
            i = j + 2
    tables = []
    for k, start_offset in enumerate(starts):
        end_offset = starts[k + 1] if k + 1 < len(starts) else end
        tables.append((start_offset, parse_run(data, start_offset, end_offset)))
    return tables


def sizes(entries):
    regs = {}
    for addr, value, _ in entries:
        regs.setdefault(addr, value)
    pair = lambda reg: (regs.get(reg, 0) << 8) | regs.get(reg + 1, 0)
    return {
        "width": pair(0x034C), "height": pair(0x034E), "line": pair(0x0342),
        "x0": pair(0x0344), "y0": pair(0x0346),
        "x1": pair(0x0348), "y1": pair(0x034A),
    }


def mode_name(index, shape):
    return "vs6955_mode%d_%dx%d" % (index, shape["width"], shape["height"])


def emit(path, init, modes):
    text = [
        "/* SPDX-License-Identifier: GPL-2.0-only */",
        "/* Private vendor register data: diagnostic input, do not commit. */",
        "/* Input SHA-256: " + DCC_SHA256 + " */",
        "#ifndef VS6955_REGISTERS_GENERATED_H",
        "#define VS6955_REGISTERS_GENERATED_H",
        "",
        "static struct msm_camera_i2c_reg_array vs6955_init[] = {",
    ]
    text += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */" % (a, v, o) for a, v, o in init]
    text += ["};", ""]
    for index, (offset, entries) in enumerate(modes):
        shape = sizes(entries)
        text.append("/* mode %d @0x%05x: %dx%d, line_length %d, crop (%d,%d)-(%d,%d) */" %
                    (index, offset, shape["width"], shape["height"], shape["line"],
                     shape["x0"], shape["y0"], shape["x1"], shape["y1"]))
        text.append("static struct msm_camera_i2c_reg_array %s[] = {" % mode_name(index, shape))
        text += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */" % (a, v, o) for a, v, o in entries]
        text += ["};", ""]
    text += ["#endif /* VS6955_REGISTERS_GENERATED_H */", ""]
    Path(path).write_text("\n".join(text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dcc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.dcc.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != DCC_SHA256:
        parser.exit(1, "Unexpected VS6955 DCC: %s\n" % digest)
    init = parse_run(data, INIT_START, INIT_END)
    modes = parse_modes(data)
    if len(init) != 805 or len(modes) != 8:
        parser.exit(1, "Expected 805 init writes and 8 modes, got %d/%d\n" % (len(init), len(modes)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    emit(temporary, init, modes)
    temporary.replace(args.output)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
