#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Inspect a pinned private Nubia fixture; never load it or write sensor registers.

JSON includes extracted vendor programs: save it outside Git. This is an
offline comparison, not a generator for the GPL production sensor module.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

FIXTURE_SHA256 = "a3f4a7347533fdf04c389d5c328bf22c91cad96901b2629512ca53dc0744490c"


class Elf:
    def __init__(self, data):
        self.data = data
        if data[:7] != b"\x7fELF\x01\x01\x01" or self.unpack("<H", 18)[0] != 40:
            raise ValueError("Expected little-endian ARM32 ELF")
        phoff = self.unpack("<I", 28)[0]
        phsize, phnum = self.unpack("<HH", 42)
        self.loads = []
        for i in range(phnum):
            kind, offset, address, _, size, _, _, _ = self.unpack("<8I", phoff + i * phsize)
            if kind == 1:
                self.loads.append((offset, address, size))
        shoff = self.unpack("<I", 32)[0]
        shsize, shnum = self.unpack("<HH", 46)
        self.relative = set()
        for i in range(shnum):
            _, kind, _, _, offset, size, _, _, _, stride = self.unpack("<10I", shoff + i * shsize)
            if kind == 9:  # SHT_REL; validate pointer records against R_ARM_RELATIVE.
                if stride != 8 or size % stride:
                    raise ValueError("Unexpected relocation records")
                for pos in range(offset, offset + size, stride):
                    target, info = self.unpack("<II", pos)
                    if info == 23:
                        self.relative.add(target)

    def unpack(self, fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(self.data):
            raise ValueError("ELF record outside file")
        return struct.unpack_from(fmt, self.data, offset)

    def file_offset(self, address, length):
        for offset, base, size in self.loads:
            if base <= address and address + length <= base + size:
                return offset + address - base
        raise ValueError("Pointer outside file-backed LOAD segments")

    def programs(self):
        result = []
        for offset, address, size in self.loads:
            for pos in range(offset, offset + size - 19, 4):
                if address + pos - offset not in self.relative:
                    continue
                pointer, count, atype, dtype, delay = self.unpack("<IH2xIIH", pos)
                if not (40 <= count <= 1000 and atype == 2 and dtype == 1 and delay <= 100):
                    continue
                start = self.file_offset(pointer, count * 8)
                entries = [self.unpack("<HHI", start + i * 8) for i in range(count)]
                if any(a == 0 or v > 255 or wait > 100 for a, v, wait in entries):
                    raise ValueError("Unexpected byte-data register program")
                result.append({"record_offset": pos, "table_offset": start,
                               "count": count, "delay_ms": delay, "entries": entries})
        return sorted(result, key=lambda p: p["record_offset"])


def word(registers, address):
    if address not in registers or address + 1 not in registers:
        return None
    return registers[address] << 8 | registers[address + 1]


def summary(entries):
    r = {a: v for a, v, _ in entries}
    return {"width": word(r, 0x034c), "height": word(r, 0x034e),
            "line_length": word(r, 0x0342), "frame_length": word(r, 0x0340),
            "extclk_8_8": word(r, 0x0136)}


def smia_programs(header):
    result = []
    for name in ("smia_idle", "smia_mode4"):
        match = re.search(r"\b" + name + r"\[\] = \{(.*?)^\};", header, re.M | re.S)
        if not match:
            raise ValueError("Missing source program " + name)
        result.extend((int(a, 16), int(v, 16), 0) for a, v in re.findall(
            r"\{(0x[0-9a-f]+), (0x[0-9a-f]+), 0\}", match[1]))
    return result


def mode_metadata(elf, modes):
    # This pinned older ABI uses 48-byte output records, not angler's 56.
    # Cross-check every record against its independent register program.
    start, stride = 0x761c, 48
    result = []
    for i, mode in enumerate(modes):
        fields = elf.unpack("<4HIIH2xffI", start + i * stride)
        width, height, line, frame, vt, output, binning, minimum, maximum, kind = fields
        actual = summary(mode["entries"])
        if (width, height, line, frame) != tuple(actual[k] for k in
                ("width", "height", "line_length", "frame_length")):
            raise ValueError("Output metadata disagrees with mode register geometry")
        if vt != 600000000 or output != 568000000 or not 0 < minimum <= maximum <= 121:
            raise ValueError("Unexpected output clock/FPS metadata")
        result.append({"mode": i, "record_offset": start + i * stride,
                       "width": width, "height": height, "line_length": line,
                       "frame_length": frame, "vt_pixel_clock_hz": vt,
                       "output_pixel_clock_hz": output, "binning_factor": binning,
                       "advertised_minimum_fps": minimum, "advertised_maximum_fps": maximum,
                       "fps_from_vt_and_geometry": vt / (line * frame), "kind": kind})
    return result


def inspect(blob, header):
    data = blob.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != FIXTURE_SHA256:
        raise ValueError("Unexpected Nubia fixture SHA-256")
    elf = Elf(data)
    programs = elf.programs()
    if len(programs) != 10 or programs[0]["count"] != 413:
        raise ValueError("Expected one init and nine mode records")
    init = programs[0]
    modes = programs[1:]
    if init["entries"][:2] != [(0x0136, 0x18, 0), (0x0137, 0, 0)]:
        raise ValueError("Unexpected EXTCLK prologue")
    if [summary(p["entries"])["width"] for p in modes] != [
            5344, 4272, 2672, 2136, 2136, 2672, 1316, 2136, 5344]:
        raise ValueError("Unexpected mode geometry/order")
    metadata = mode_metadata(elf, modes)
    source = smia_programs(header.read_text())
    sr = {a: v for a, v, _ in source}
    # Mode 0 has the 173 writes and 5344x4016 geometry reported in A11 logs.
    # Counts/geometry identify a candidate; logs alone cannot prove every write.
    reference = {a: v for a, v, _ in init["entries"] + modes[0]["entries"]}
    differences = [{"address": f"0x{a:04x}", "nubia": reference.get(a), "smia": sr.get(a)}
                   for a in sorted(reference.keys() | sr.keys()) if reference.get(a) != sr.get(a)]
    for i, p in enumerate(programs):
        p["name"] = "init" if i == 0 else f"mode{i - 1}"
        p["summary"] = summary(p["entries"])
        p["entries"] = [{"address": f"0x{a:04x}", "value": f"0x{v:02x}", "delay_ms": d}
                        for a, v, d in p["entries"]]
    timing = range(0x080a, 0x081a)
    return {"status": "offline private fixture comparison; no hardware writes",
            "fixture_sha256": digest,
            "smia_header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
            "programs": programs, "mode_metadata": metadata,
            "smia_mode_summary": summary(source),
            "differences": differences,
            "dphy_timing": [{"address": f"0x{a:04x}", "nubia": reference.get(a), "smia": sr.get(a)}
                            for a in timing],
            "limits": ["No reconstruction of missing runtime logs or dynamic writes",
                       "D-PHY timing absence is a candidate cause, not a proven fault",
                       "Different geometries/clocks: do not transplant timing values blindly",
                       "No vendor register table is added to production source"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("blob", type=Path)
    parser.add_argument("--smia-header", type=Path,
                        default=Path(__file__).with_name("smia_registers.h"))
    args = parser.parse_args()
    try:
        result = inspect(args.blob, args.smia_header)
    except (ValueError, OSError, struct.error) as error:
        parser.exit(1, f"Cannot inspect reference: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
