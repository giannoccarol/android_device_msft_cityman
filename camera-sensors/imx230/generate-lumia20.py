#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate the private Lumia 20 MP IMX230 mode tables from the stock CDCC.

The init is the public smia_idle table already in smia_registers.h; this
generator only extracts the Cityman mode deltas. The DCC input is
proprietary and stays outside Git.

It also derives the opt-in 2x2-binned mode (LUMIA20-BINNED.md): mode 0 with
the SMIA++ binning and output-size registers changed, so the PLL, link rate
and readout window stay those of the working full-resolution mode. The
derivation is cross-checked against two binned tables of the same DCC.
"""
import argparse
import hashlib
import sys
from pathlib import Path

DCC_SHA256 = "6fcbdf9d37893224bf864bb16979f631a919cfad97a809788e0da8a161a4d988"
WRITES = 73
MODES = {
    "lumia20_mode0": 0x09D3,  # 5344x3744, 20 MP
    "lumia20_mode1": 0x0AB1,  # 5344x3008, 16 MP 16:9
}
# Reference tables for the binned derivation only; never emitted.
BINNED_FULL_WIDTH = 0x0C6D  # 2672x1504, 2x2 binning of x 0-5343, y 504-3511
BINNED_FULL_HEIGHT = 0x0D4B  # 2496x1872, 2x2 binning of x 176-5167, y 136-3879

# 2x2 binning of the mode 0 window (x 0-5343, y 136-3879) -> 2672x1872.
BINNED_WIDTH, BINNED_HEIGHT = 2672, 1872
BINNED_DELTAS = {
    0x034C: BINNED_WIDTH >> 8, 0x034D: BINNED_WIDTH & 0xFF,    # x_output_size
    0x034E: BINNED_HEIGHT >> 8, 0x034F: BINNED_HEIGHT & 0xFF,  # y_output_size
    0x040C: BINNED_WIDTH >> 8, 0x040D: BINNED_WIDTH & 0xFF,    # digital crop width
    0x040E: BINNED_HEIGHT >> 8, 0x040F: BINNED_HEIGHT & 0xFF,  # digital crop height
    0x0900: 0x01,  # binning_mode: on
    0x0901: 0x22,  # binning_type: 2x2
    0x3A25: BINNED_HEIGHT >> 8, 0x3A26: BINNED_HEIGHT & 0xFF,  # mirror of y_output_size
    0x3A38: 0x01,  # vendor mirror of the binning factor (0, 1 = 2x2, 2 = 4x4)
}
# Registers that only depend on the PLL/link rate or on the vertical window.
PLL_LINK = {0x0301, 0x0303, 0x0305, 0x0307, 0x0309, 0x030B, 0x030D, 0x030F, 0x0820, 0x0821}
VERTICAL = {0x0346, 0x0347, 0x034A, 0x034B, 0x034E, 0x034F, 0x040E, 0x040F,
            0x3A25, 0x3A26, 0x3A31, 0x3A32, 0x3A35, 0x3A36}


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


def derive_binned(data, mode0):
    """Return mode 0 with BINNED_DELTAS applied, checked against the DCC."""
    full_width = parse_run(data, BINNED_FULL_WIDTH, WRITES)
    full_height = parse_run(data, BINNED_FULL_HEIGHT, WRITES)
    order = [a for a, _, _ in mode0]
    if [a for a, _, _ in full_width] != order or [a for a, _, _ in full_height] != order:
        raise ValueError("DCC mode tables no longer share one register order")
    if not set(BINNED_DELTAS) <= set(order):
        raise ValueError("A binned delta register is missing from mode 0")
    derived = [(a, BINNED_DELTAS.get(a, v), o) for a, v, o in mode0]
    # Independent composition: the Windows full-width binned mode, with the
    # PLL/link of mode 0 and the vertical window of the 1872-row binned mode.
    expected = []
    for (a, v0, _), (_, vw, _), (_, vh, _) in zip(mode0, full_width, full_height):
        expected.append(v0 if a in PLL_LINK else vh if a in VERTICAL else vw)
    if [v for _, v, _ in derived] != expected:
        raise ValueError("Binned derivation disagrees with the DCC binned modes")
    return derived


def emit_table(lines, name, entries, offset, array):
    macro = name.upper() + "_WRITES"
    lines.append("/* %s: %d writes at 0x%05x */" % (name, len(entries), offset))
    lines.append("#define %s \\" % macro)
    lines += ["    {0x%04x, 0x%02x, 0},  /* @0x%05x */ \\" % (a, v, o) for a, v, o in entries]
    lines.append("")
    if array:
        lines.append("static struct msm_camera_i2c_reg_array %s[] = {" % name)
        lines += ["    " + macro, "};"]
    lines.append("")


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
    tables = {name: parse_run(data, offset, WRITES) for name, offset in MODES.items()}
    for name, offset in MODES.items():
        emit_table(lines, name, tables[name], offset, True)
    # Macro only: no array, so the default build stays unchanged.
    emit_table(lines, "lumia20_binned", derive_binned(data, tables["lumia20_mode0"]),
               MODES["lumia20_mode0"], False)
    lines += ["#endif /* LUMIA20_REGISTERS_GENERATED_H */", ""]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text("\n".join(lines))
    temporary.replace(args.output)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
