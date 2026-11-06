#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate the private iris-camera register header from the stock Lumia
CDCC 0C086101.dcc (module "Jopo", Foxconn; Toshiba SMIA sensor).

The DCC is proprietary input and stays outside Git. Layout, as used by the
Windows SMIA++ wrapper (cam_drv) and shared with the other Lumia DCCs:

  0x00  u32 BE format (0x1f), u32 BE file size, 32-byte name "0C086101"
  0x34  u16 LE block count, then count x (u16 BE id, u32 BE length)
        blocks follow back to back after the table
  id 0x0000  sensor init: 16-bit register, 8-bit value triplets
  id 0xff0N  sensor mode N: same triplets (the iris DCC has one mode)
  id 0x0005  emulated SMIA limits (frame format, gain model, PLL limits)
  id 0x000e  initial AE settings (350 us exposure, 1920x1088)
  id 0xfe9b / 0x0006  ISP tuning ("Jopo_Foxconn mp1.0" / "cs1.0")
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

DCC_SHA256 = "656c1dd9c1d5a818a232319ef16b8ea7fa1936ce9ea40ed9c241ddbcaee86a85"
EXPECTED_INIT = 87
EXPECTED_MODE = 30


def blocks(data):
    count = struct.unpack_from("<H", data, 0x34)[0]
    table = 0x38
    position = table + 6 * count
    out = {}
    for i in range(count):
        block_id, length = struct.unpack_from(">HI", data, table + 6 * i)
        out[block_id] = (position, data[position:position + length])
        position += length
    return out


def triplets(offset, payload):
    out = []
    for i in range(0, len(payload) - 2, 3):
        address = (payload[i] << 8) | payload[i + 1]
        if address == 0 and payload[i + 2] == 0:
            break
        out.append((address, payload[i + 2], offset + i))
    return out


def emit(path, init, mode):
    text = [
        "/* SPDX-License-Identifier: GPL-2.0-only */",
        "/* Private vendor register data: diagnostic input, do not commit. */",
        "/* Input SHA-256: " + DCC_SHA256 + " */",
        "#ifndef CK71IR_REGISTERS_GENERATED_H",
        "#define CK71IR_REGISTERS_GENERATED_H",
        "",
        "static struct msm_camera_i2c_reg_array ck71ir_init[] = {",
    ]
    text += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */" % entry for entry in init]
    text += ["};", "", "/* mode 0: 1920x1088 RAW10, one CSI-2 lane, 700.8 Mbps */",
             "static struct msm_camera_i2c_reg_array ck71ir_mode0_1920x1088[] = {"]
    text += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */" % entry for entry in mode]
    text += ["};", "", "#endif /* CK71IR_REGISTERS_GENERATED_H */", ""]
    Path(path).write_text("\n".join(text))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dcc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.dcc.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != DCC_SHA256:
        parser.exit(1, "Unexpected iris DCC: %s\n" % digest)
    table = blocks(data)
    if 0x0000 not in table or 0xff00 not in table or 0xff01 in table:
        parser.exit(1, "Expected one init block and exactly one mode block\n")
    init = triplets(*table[0x0000])
    mode = triplets(*table[0xff00])
    if len(init) != EXPECTED_INIT or len(mode) != EXPECTED_MODE:
        parser.exit(1, "Expected %d init and %d mode writes, got %d/%d\n" %
                    (EXPECTED_INIT, EXPECTED_MODE, len(init), len(mode)))
    regs = {a: v for a, v, _ in mode}
    word = lambda r: (regs[r] << 8) | regs[r + 1]
    if (word(0x034c), word(0x034e), word(0x0342), regs[0x0114]) != (1920, 1088, 2100, 0):
        parser.exit(1, "Mode 0 is not 1920x1088, line 2100, one lane\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    emit(temporary, init, mode)
    temporary.replace(args.output)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
