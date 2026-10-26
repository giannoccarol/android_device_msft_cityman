#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate a private diagnostic profile from the pinned, locally supplied ELF.

The generated vendor register data has no GPL license assertion. It must stay
outside Git. No executable code or input sensor descriptor is copied.
"""
import argparse
import importlib.util
from pathlib import Path
import sys

# Avoid creating __pycache__ inside an Android source tree mounted read-only.
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("nubia_inspector", HERE / "inspect-nubia-registers.py")
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


def generate(blob):
    result = inspector.inspect(blob, HERE / "smia_registers.h")
    selected = [result["programs"][0], result["programs"][1]]
    if any(int(r["address"], 16) in (0, 0x0100, 0x0107) for p in selected for r in p["entries"]):
        raise ValueError("Unexpected identity, stream or address-change write")
    text = ["/* Private vendor register data: diagnostic input, do not commit. */",
            "/* Input SHA-256: " + result["fixture_sha256"] + " */"]
    for program, name in zip(selected, ("nubia24_init", "nubia24_mode0")):
        text.append("static struct msm_camera_i2c_reg_array " + name + "[] = {")
        text.extend("    {%s, %s, %d}," % (r["address"], r["value"], r["delay_ms"])
                    for r in program["entries"])
        text.append("};")
    return "\n".join(text) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("blob", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        source = generate(args.blob)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix(args.output.suffix + ".tmp")
        temporary.write_text(source)
        temporary.replace(args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Cannot generate Nubia profile: {error}\n")


if __name__ == "__main__":
    main()
