#!/usr/bin/env python3
"""Emit C source for the talkman AF search tuning libraries.

Runs BY HAND; output is checked in, so the build compiles ordinary C and reads
no vendor blob.  Same contract as chromatix/gen-chromatix-c.py:

    ./gen-af-c.py verify    payload still matches donor + declared overrides
    ./gen-af-c.py emit      regenerate data/*.inc

Read af_tuning_talkman.h first -- particularly the two things that must NOT be
rescaled (the micrometre scan-step tables) or inverted (CAF/TAF ends).
"""
import os, re, struct, subprocess, sys

HERE  = os.path.dirname(os.path.abspath(__file__))
DONOR = os.path.normpath(os.path.join(HERE, "../../../../../../vendor/msft/cityman/proprietary/vendor/lib"))
MODES = ["camera", "camcorder"]
TUNING_AT = 8            # the struct starts at .data + 8
SIZE      = 4992

# The donor's step table ends at 324 and ours at 268 (S38-S39), so every field
# expressed in actuator steps scales by this and nothing else does.
STEP_SCALE = (268, 324)
def S(v):
    return round(v * STEP_SCALE[0] / STEP_SCALE[1])

# offset -> (name, C type, count).  From af_set_debug_print_af_tuning_parms and
# af_util_is_low_light in libmmcamera2_q3a_core.so, every offset checked against
# what the daemon printed for the donor.
FIELDS = [
 (0x048, "af_process_type",       "uint16_t", 1),
 (0x04a, "pos_near",              "uint16_t", 1),
 (0x04c, "pos_def_mac",           "uint16_t", 1),
 (0x04e, "pos_bound",             "uint16_t", 1),
 (0x050, "pos_def_norm",          "uint16_t", 1),
 (0x052, "pos_far",               "uint16_t", 1),
 (0x054, "pos_hypf",              "uint16_t", 1),
 (0x056, "pos_mac_rgn",           "uint16_t", 1),
 (0x06c, "low_light_gain_th",     "float",    1),
 (0x098, "search_step_size",      "uint16_t", 1),
 (0x0e8, "num_gross_steps",       "uint16_t", 1),
 (0x0ea, "num_fine_steps",        "uint16_t", 1),
 (0x0ec, "num_fine_search_pts",   "uint16_t", 1),
 (0x154, "step_index",            "uint16_t", 12),
 (0x16c, "actuator_type",         "uint16_t", 1),
 (0x16e, "is_hys_comp_needed",    "uint16_t", 1),
 (0x170, "step_index_per_um",     "uint16_t", 1),
 # Eight 5-entry scan-step tables, in MICROMETRES -- see the header.
 (0x172, "scan_step_um",          "uint16_t", 40),
 (0x1c2, "PAAF_enable",           "uint16_t", 1),
 (0x1c4, "sw_flat_inc_thres",     "float",    1),
 (0x1c8, "sw_flat_dec_thres",     "float",    1),
 (0x1cc, "sw_macro_thres",        "float",    1),
 (0x1d0, "sw_drop_thres",         "float",    1),
 (0x1d4, "sw_hist_dec_dec_thres", "uint32_t", 1),
 (0x1d8, "sw_hist_inc_dec_thres", "uint32_t", 1),
 (0x2bc, "hw_flat_inc_thres",     "float",    1),
 (0x2c0, "hw_flat_dec_thres",     "float",    1),
 (0x2c4, "hw_macro_thres",        "float",    1),
 (0x2c8, "hw_drop_thres",         "float",    1),
 (0x2cc, "hw_hist_dec_dec_thres", "uint32_t", 1),
 (0x2d0, "hw_hist_inc_dec_thres", "uint32_t", 1),
 (0x3d4, "CAF_far_end",           "uint16_t", 1),
 (0x3d6, "CAF_near_end",          "uint16_t", 1),
 (0x3d8, "TAF_far_end",           "uint16_t", 1),
 (0x3da, "TAF_near_end",          "uint16_t", 1),
 (0x3dc, "srch_rgn_1",            "uint16_t", 1),
 (0x3de, "srch_rgn_2",            "uint16_t", 1),
 (0x3e0, "srch_rgn_3",            "uint16_t", 1),
 (0x3e2, "fine_srch_rgn",         "uint16_t", 1),
 (0x3e4, "far_zone",              "uint16_t", 1),
 (0x3e6, "near_zone",             "uint16_t", 1),
 (0x3e8, "mid_zone",              "uint16_t", 1),
 (0x3ee, "init_pos",              "uint16_t", 1),
]

# Deliberately not the donor's value.  Everything here is either a step-valued
# field rescaled to this module's 268-step table, or a documented decision.
OVERRIDES = {
 "pos_near":     (S(0),   "step-valued: rescaled 324 -> 268 (S38)"),
 "pos_def_mac":  (S(124), "step-valued: rescaled 324 -> 268"),
 "pos_bound":    (S(260), "step-valued: rescaled 324 -> 268"),
 "pos_def_norm": (S(300), "step-valued: rescaled 324 -> 268"),
 "pos_far":      (S(324), "step-valued: rescaled 324 -> 268"),
 "pos_hypf":     (S(300), "step-valued: rescaled 324 -> 268"),
 "pos_mac_rgn":  (S(188), "step-valued: rescaled 324 -> 268"),
 "step_index":   (tuple(round(i * 268 / 11) for i in range(12)),
                  "the step grid for this module's 268-step table"),
 "step_index_per_um": (1,
   "a u16, and the scan-step tables are in micrometres, so one actuator step "
   "must be one micrometre.  268 steps of 2 codes spans the NVM's 900..1436, "
   "i.e. 268 um, infinity to 80 mm (S39)."),
 "PAAF_enable":  (0, "no phase-detect AF path on this port"),
}

CTYPE_SIZE = {"uint8_t":1, "uint16_t":2, "uint32_t":4, "float":4}
PACK = {"uint16_t":"<H", "uint32_t":"<I", "float":"<f"}

def payload(path):
    out = subprocess.run(["readelf","-S",path],capture_output=True,text=True).stdout
    for l in out.splitlines():
        if re.search(r'\.data\s+PROGBITS', l):
            p=l.split(); k=p.index('PROGBITS')
            off,size = int(p[k+2],16), int(p[k+3],16)
            d = open(path,'rb').read()[off+TUNING_AT:off+size]
            if len(d) != SIZE:
                raise SystemExit("%s: tuning is %d bytes, expected %d" % (path,len(d),SIZE))
            return d
    raise SystemExit("no .data in %s" % path)

def layout():
    out, pos = [], 0
    for off, name, ctype, count in sorted(FIELDS):
        w = CTYPE_SIZE[ctype] * count
        if off < pos:
            raise SystemExit("field %s at 0x%x overlaps 0x%x" % (name, off, pos))
        if off > pos:
            out.append(("gap", pos, "_gap_%04x" % pos, off - pos))
        out.append(("field", off, name, (ctype, count)))
        pos = off + w
    if pos < SIZE:
        out.append(("gap", pos, "_gap_%04x" % pos, SIZE - pos))
    return out

def override_bytes(name, ctype, count):
    if name not in OVERRIDES:
        return None
    v = OVERRIDES[name][0]
    if count > 1:
        if len(v) != count:
            raise SystemExit("%s: override has %d values, field holds %d" % (name,len(v),count))
        return b"".join(struct.pack(PACK[ctype], x) for x in v)
    return struct.pack(PACK[ctype], v)

def f32(v):
    bits = struct.pack("<f", v)
    for p in range(1, 10):
        t = "%.*g" % (p, v)
        if struct.pack("<f", float(t)) == bits:
            break
    if not any(c in t for c in ".en"):
        t += ".0"
    return t + "f"

def fmt(ctype, count, raw):
    if ctype == "float":
        return f32(struct.unpack_from("<f", raw)[0])
    vals = [struct.unpack_from(PACK[ctype], raw, i*CTYPE_SIZE[ctype])[0] for i in range(count)]
    if count == 1:
        return str(vals[0])
    L, per = [], 12 if ctype == "uint16_t" else 8
    for i in range(0, count, per):
        L.append("\t\t" + ", ".join(str(x) for x in vals[i:i+per]))
    return "{\n" + ",\n".join(L) + "\n\t}"

def emit(data):
    L = []
    for kind, off, name, spec in layout():
        if kind == "gap":
            L.append("\t.%s = {" % name)
            for i in range(0, spec, 16):
                L.append("\t\t" + " ".join("0x%02x," % b for b in data[off+i:off+min(i+16,spec)]))
            L.append("\t},")
            continue
        ctype, count = spec
        w = CTYPE_SIZE[ctype] * count
        ov = override_bytes(name, ctype, count)
        if ov is not None:
            L.append("\t/* talkman: %s */" % OVERRIDES[name][1])
        L.append("\t.%s = %s," % (name, fmt(ctype, count, ov if ov is not None else data[off:off+w])))
    return "\n".join(L)

def expected(data):
    want = bytearray(data)
    n = 0
    for kind, off, name, spec in layout():
        if kind == "gap":
            continue
        ctype, count = spec
        ov = override_bytes(name, ctype, count)
        if ov is not None:
            want[off:off+len(ov)] = ov
            n += 1
    return bytes(want), n

def struct_decl():
    L = ["typedef struct {"]
    for kind, off, name, spec in layout():
        if kind == "gap":
            L.append("\tuint8_t  %-24s[0x%x];\t/* 0x%04x undecoded */" % (name, spec, off))
        else:
            ctype, count = spec
            L.append("\t%-9s %-24s%s;\t/* 0x%04x */" % (ctype, name,
                     "[%d]" % count if count > 1 else "", off))
    L.append("} af_tuning_t;\n")
    for kind, off, name, spec in layout():
        L.append("AF_CHECK_OFF(af_tuning_t, %s, 0x%04x, %s);" % (name, off, name.strip('_')))
    L.append("AF_STATIC_ASSERT(sizeof(af_tuning_t) == %d, af_tuning_size);" % SIZE)
    return "\n".join(L)

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "verify"
    rc = 0
    for m in MODES:
        data = payload(os.path.join(DONOR, "libactuator_lc898212xd_%s.so" % m))
        want, n = expected(data)
        if mode == "verify":
            print("  %-10s %d bytes  ok  (%d field(s) deliberately overridden)" % (m, len(data), n))
        else:
            with open(os.path.join(HERE, "data", "af_%s.inc" % m), "w") as f:
                f.write("/* GENERATED by gen-af-c.py -- do not edit by hand. */\n")
                f.write(emit(data) + "\n")
            print("  wrote data/af_%s.inc (%d bytes, %d overridden)" % (m, len(data), n))
    if mode == "emit":
        with open(os.path.join(HERE, "af_tuning_structs.h"), "w") as f:
            f.write("/* GENERATED by gen-af-c.py -- do not edit by hand. */\n"
                    "#ifndef AF_TUNING_STRUCTS_H\n#define AF_TUNING_STRUCTS_H\n"
                    '#include "af_tuning_talkman.h"\n\n')
            f.write(struct_decl())
            f.write("\n\n#endif\n")
        print("  wrote af_tuning_structs.h")
    return rc

sys.exit(main())
