#!/usr/bin/env python3
"""Emit C source for the talkman chromatix libraries.

This runs BY HAND, not from the build.  Its output is checked in, so the build
reads no vendor blob and nothing is patched: `Android.mk` compiles ordinary C.

    ./gen-chromatix-c.py emit          regenerate the .inc/.c sources
    ./gen-chromatix-c.py emit cpp_     regenerate only the .inc files whose
                                       name starts with cpp_ (see below)
    ./gen-chromatix-c.py verify        prove the sources still reproduce the
                                       donor payload byte for byte

WHY A GENERATOR AT ALL
----------------------
The payload is ~250 KB across seven distinct tuning structs.  Typing that by
hand is not review, it is transcription error.  The generator writes it once
and `verify` keeps it honest afterwards: every decoded field that moves out of
a `_gap_*` array into a named one must leave the emitted bytes unchanged, and
verify is what proves it did.

WHAT IS AND IS NOT CLAIMED
--------------------------
The container is fully understood (see chromatix_talkman.h).  The payload is
not: only the fields in FIELDS below are decoded, and the values are still the
imx377 donor's tuning except where a field has been measured for this module.
A `_gap_*` array is undecoded data, labelled with its own struct offset.
"""
import os, re, subprocess, sys, struct

HERE   = os.path.dirname(os.path.abspath(__file__))
# CHX_DONOR points at the donor libraries when this tree is not inside the
# Android source tree (a separate worktree, for example).
DONOR  = os.environ.get("CHX_DONOR") or os.path.normpath(os.path.join(HERE, "../../../../../../vendor/msft/cityman/proprietary/vendor/lib"))
STRUCT_AT = 0x3004        # file offset of the struct; .data is at 0x3000

# The ten names sensor_lib_imx230.c's .chromatix table asks for.
NAMES = ["common", "snapshot", "default_video", "liveshot", "postproc",
         "cpp_preview", "cpp_snapshot", "cpp_video", "cpp_uhd_video",
         "cpp_liveshot"]

# Payload size -> C struct type.  Four distinct tuning structs ship.
# Keyed on STRUCT size, i.e. .data size minus the 4-byte self-pointer that
# precedes it.  Getting this wrong shifts every field by four and still
# round-trips, because emit and rebuild share the indexing -- the AEC flux
# tripwire is what catches it.
CLASS = {67064: "chx_vfe_t", 34892: "chx_cpp_t",
         44140: "chx_common_t", 2524: "chx_postproc_t"}

# ---------------------------------------------------------------- decode ----
# Decoded fields, per struct class: (struct offset, C field name, C type).
# Everything not listed lands in a _gap_<offset> byte array.
#
# Every entry here is an offset this port established independently and can
# point at evidence for; none is guessed from a header we do not have.
# The ISP linearization knee ladder (S120/S122/S123), in the COMMON chromatix.
#
# 24 contiguous groups of 0x48 bytes from struct 0x0864 = six trigger entries
# x four Bayer channels.  Each group is 8 u16 knee INPUT positions at +0x00,
# nine u16 outputs at +0x10 and eight float slopes at +0x28; only the inputs
# are named here, so the outputs and slopes stay byte-for-byte the donor's.
#
# linearization40 hands its four per-channel arrays to the VFE's four register
# groups in the order 0, 2, 3, 1 -- that is, group 0 gets array 0, group 1 gets
# array 2, group 2 gets array 3 and group 3 gets array 1.  The VFE's groups
# (0x0380/0x0390/0x03a0/0x03b0) are R, Gb, B, Gr for this RGGB sensor (S123,
# measured by lifting one group at a time), so the arrays here are in the
# canonical Bayer order R, Gr, Gb, B.
#
# That direction was established by MEASUREMENT, not by reading the disassembly
# note, which is easy to read backwards: a first build assigned the four a
# distinguishable set of black levels and the registers were read back live.
# The corroboration is that the pedestals then fall in Bayer order and match
# S122's measurement of them term for term.
LIN_BASE, LIN_STRIDE, LIN_GROUPS = 0x0864, 0x48, 24
LIN_CH_NAME = ("r", "gr", "gb", "b")
LIN_KNEE_FIELDS = [
    (LIN_BASE + g * LIN_STRIDE, "lin_knee_in_t%d_%s" % (g // 4, LIN_CH_NAME[g % 4]),
     "uint16_t", 8)
    for g in range(LIN_GROUPS)
]

# CPP wavelet denoise noise profiles, in the CPP chromatix.
#
# Six gain trigger entries 0xe0 apart from struct 0x6fbc; each holds its gain
# range as two floats at +0x04 (2..3.99, 4..7.99, ... 64..88), then three
# noise profiles of four floats (one per wavelet level, finest first): luma at
# +0x24, Cb at +0x44, Cr at +0x64.  The weights that follow (3 / 15 / 30) stay
# the donor's.  The layout is identical in the preview, snapshot, video,
# liveshot and UHD video payloads.
WNR_PLANES = (("y", 0x00), ("cb", 0x20), ("cr", 0x40))
WNR_FIELDS = [(0x6fe0 + 0xe0 * t + d, "wnr_%s_t%d" % (pl, t), "float", 4)
              for t in range(6) for pl, d in WNR_PLANES]

FIELDS = {
 "chx_vfe_t": [
   (0x0000, "version",                   "uint8_t",          4),
   # S169: AWB's own log prints this back as (rg,bg) = (1/r, 1/b).  The shipped
   # value is Hill's measured (1.9231, 1.0, 1.6750), not the donor's
   # (1.7339, 1.0, 2.5300) -- see mk-chromatix notes and live-csi-plan S169.
   (0x6eac, "led_ref_white_point",       "chx_white_point_t", 1),
   (0x6eb8, "led_ref_white_point_dup",   "chx_white_point_t", 1),
   # S153: aec_process_led_adjust_exp_settings reads ctx[0x1810] as the
   # selector and ctx[0x1804]/ctx[0x1800] as the ratio; those map here.
   # The colour correction matrix table: eight 3x3 matrices, one per
   # illuminant trigger, stride 0x2c (9 floats + 8 bytes of trigger data).
   # Located by SHAPE, not by a literal search: the final Q7 register values
   # the HAL writes (181 -18 -36 / -89 216 1 / -93 -1 222) appear nowhere in
   # the file, because color_correct40_interpolate() blends two of these
   # entries at runtime -- it reads 9 consecutive floats at +0x00..+0x20 from
   # each and writes the interpolated result.  Every entry here has rows
   # summing to 1.0 with a dominant diagonal, and the last two are duplicates,
   # the same duplication S169 found for the LED white point.
   (0x40a8, "ccm_0", "float", 9),
   (0x40d4, "ccm_1", "float", 9),
   (0x4100, "ccm_2", "float", 9),
   (0x412c, "ccm_3", "float", 9),
   (0x4158, "ccm_4", "float", 9),
   (0x4184, "ccm_5", "float", 9),
   (0x41b0, "ccm_6", "float", 9),
   (0x41dc, "ccm_7", "float", 9),
   (0x7c08, "aec_flash_flux_selector",   "uint32_t",         1),
   (0xd4e8, "aec_flash_flux_denom",      "float",            1),
   (0xd4ec, "aec_flash_flux_ratio",      "float",            1),
 ],
 "chx_cpp_t":      [(0x0000, "version", "uint8_t", 4)] + WNR_FIELDS,
 "chx_common_t":   [(0x0000, "version", "uint8_t", 4)] + LIN_KNEE_FIELDS,
 "chx_postproc_t": [(0x0000, "version", "uint8_t", 4)],
}

# Fields deliberately NOT the donor's value.  Each needs a measurement and a
# record; `verify` prints them so a reviewer sees exactly what this port changed
# and why, instead of a diff against a binary.
OVERRIDES = {
 ("chx_vfe_t", "ccm_0"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "the Lumia 950's OWN Daylight colour matrix, from this module's WP calibration (0AEACA05.dcc, |ISP_DNG| at 0x0d4b3: two DNG ColorMatrix 3x3s at scale 10000, Halogen and Daylight).  Converted with CCM = sRGB_from_XYZ * inv(ColorMatrix), row-normalised, and exact in float32 because every term is an integer over 128 (Q7 178 -34 -16 / -56 247 -63 / -10 -69 207).  Measured on one scene: identity 0.0471 saturation, the imx377 donor 0.1584, this 0.1861 -- more colour at the same neutrality (spread 1.02).  ALL EIGHT entries carry it so color_correct40_interpolate() cannot blend back towards imx377's tuning; that also makes this exactly equivalent to the kernel-side VFE_WRITE override it replaces, which forced one matrix regardless of illuminant."),
 ("chx_vfe_t", "ccm_1"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_2"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_3"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_4"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_5"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_6"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "ccm_7"):
   ((1.390625, -0.265625, -0.125, -0.4375, 1.9296875, -0.4921875, -0.078125, -0.5390625, 1.6171875),
    "same matrix; see ccm_0.  Every entry must match or the interpolation reintroduces the donor's tuning."),
 ("chx_vfe_t", "led_ref_white_point"):
   ((1.9231, 1.0, 1.6750),
    "S169: Hill's measured LED gains.  The donor's (1.7339, 1.0, 2.5300) is "
    "imx377's LED and applies 2.53x blue where this part needs 1.675x -- the "
    "flash blue cast.  Measured B/R 1.54 -> 0.91, stable across ambient."),
 ("chx_vfe_t", "led_ref_white_point_dup"):
   ((1.9231, 1.0, 1.6750), "the table holds the LED entry twice; same source."),
}

# Per-channel black level.  The donor's ladder starts at 210 (12-bit), which is
# imx377's pedestal -- 52.5 in 10-bit -- because the only chromatix on this ROM
# is imx377's.  This sensor's is DATA_PEDESTAL 64 (10-bit), and the four Bayer
# channels were measured from whole dark frames as R 256, Gr 253, Gb 256,
# B 255 (S122).  The 46 counts the donor leaves behind do not look like a grey
# lift: white balance doubles two channels and the CCM then turns a uniform
# offset O into R 2.02*O, G 0.149*O, B 2.55*O, so a lights-off room measured
# R 84 G 1.3 B 58 and read as a colour problem for weeks.
#
# Indexed by chromatix channel, in the Bayer order those arrays are stored in.
# Verified live: with these values the VFE groups read back 0x380 R 256,
# 0x390 Gb 256, 0x3a0 B 255, 0x3b0 Gr 253.
LIN_BLACK = (256, 253, 256, 255)   # R, Gr, Gb, B -- S122, whole dark frames

def lin_shift(target):
    """Move a knee ladder's start to `target`, keeping every segment width.

    The spacings are the donor's calibration of the ISP's own transfer curve
    and the slopes already loaded alongside them depend on those widths, so
    only the start may move.  This is exactly what the kernel-side VFE_WRITE
    override it replaces did to each 0x037c write."""
    def f(cur):
        d = target - cur[0]
        out = tuple(v + d for v in cur)
        # The VFE packs these as 12-bit fields; a shift that pushes the top
        # knee past 4095 would wrap and invert the curve.
        if out[-1] > 4095 or out[0] < 0:
            raise SystemExit("lin_shift: ladder %s -> %s leaves 0..4095" %
                             (cur, out))
        return out
    return f

OVERRIDES.update({
    ("chx_common_t", "lin_knee_in_t%d_%s" % (g // 4, LIN_CH_NAME[g % 4])):
      (lin_shift(LIN_BLACK[g % 4]),
       "S122/S123: black level.  The whole ladder shifts so the curve starts "
       "at this sensor's measured pedestal instead of imx377's 210; segment "
       "widths, outputs and slopes are untouched.  All six trigger entries "
       "carry it, so interpolation cannot blend the donor's value back -- the "
       "donor's own 210..215 drift across entries is imx377's dark current "
       "and unmeasured here, so it is deliberately dropped.")
    for g in range(LIN_GROUPS)
})

# Stronger CPP denoise.  The donor's profiles are imx377's noise, and this
# module is noisier at the same gain: preview and photos showed fine colour
# grain at every light level.  The scaling is the one tested on this phone with
# the Android 13 cityman camera (luma x1.4, chroma x1.6, every trigger entry):
# grain gone without visible loss of detail.  Relative, so the donor's shape
# across wavelet levels and gains survives.
def wnr_scale(k):
    return lambda cur: tuple(v * k for v in cur)

OVERRIDES.update({
    ("chx_cpp_t", "wnr_%s_t%d" % (pl, t)):
      (wnr_scale(1.4 if pl == "y" else 1.6),
       "stronger wavelet denoise, %s x%s: this module is noisier than "
       "imx377 at the same gain (scaling tested on Android 13)"
       % ("luma" if pl == "y" else "chroma", "1.4" if pl == "y" else "1.6"))
    for t in range(6) for pl, _ in WNR_PLANES
})

CTYPE_SIZE = {"uint8_t":1, "uint16_t":2, "uint32_t":4, "int32_t":4,
              "float":4, "chx_white_point_t":12}

def payload(path):
    out = subprocess.run(["readelf","-S",path], capture_output=True, text=True).stdout
    for l in out.splitlines():
        if re.search(r'\.data\s+PROGBITS', l):
            p = l.split(); k = p.index('PROGBITS')
            off, size = int(p[k+2],16), int(p[k+3],16)
            # drop the leading self-pointer: the struct starts at .data + 4
            return open(path,'rb').read()[off+4:off+size]
    raise SystemExit("no .data in %s" % path)

def layout(cls, size):
    """Interleave decoded fields with _gap_ arrays covering the rest."""
    out, pos = [], 0
    for off, name, ctype, count in sorted(FIELDS[cls]):
        width = CTYPE_SIZE[ctype] * count
        if off < pos:
            raise SystemExit("%s: field %s at 0x%x overlaps 0x%x" % (cls,name,off,pos))
        if off > pos:
            out.append(("gap", pos, "_gap_%04x" % pos, off - pos))
        out.append(("field", off, name, (ctype, count)))
        pos = off + width
    if pos < size:
        out.append(("gap", pos, "_gap_%04x" % pos, size - pos))
    return out

def emit_struct_decl(cls, size):
    L = ["typedef struct {"]
    for kind, off, name, spec in layout(cls, size):
        if kind == "gap":
            L.append("\tuint8_t  %-26s[0x%x];\t/* 0x%04x undecoded */" % (name, spec, off))
        else:
            ctype, count = spec
            arr = "[%d]" % count if count > 1 else ""
            L.append("\t%-18s %s%s;\t/* 0x%04x */" % (ctype, name, arr, off))
    L.append("} %s;" % cls)
    L.append("")
    for kind, off, name, spec in layout(cls, size):
        L.append("CHX_CHECK_OFF(%s, %s, 0x%04x, %s_%s);" % (cls, name, off, cls[:-2], name.strip('_')))
    L.append("CHX_STATIC_ASSERT(sizeof(%s) == %d, %s_size);" % (cls, size, cls[:-2]))
    return "\n".join(L)

def fmt_bytes(b, indent="\t\t"):
    L, W = [], 16
    for i in range(0, len(b), W):
        L.append(indent + " ".join("0x%02x," % x for x in b[i:i+W]))
    return "\n".join(L)

def unpack_field(ctype, count, raw):
    """A field's donor value, as a tuple, for a relative override."""
    if ctype == "uint16_t":
        return struct.unpack_from("<%dH" % count, raw)
    if ctype == "float":
        return struct.unpack_from("<%df" % count, raw)
    if ctype == "uint32_t":
        return struct.unpack_from("<I", raw)[0]
    raise SystemExit("no unpacker for %s" % ctype)

def override_bytes(cls, name, ctype, count, cur=None):
    """Packed bytes for an overridden field, or None.

    An override value may be a callable, which is handed the donor's own
    value for the field and returns the replacement.  That is how the
    linearization ladder is shifted: the knee SPACINGS are the donor's
    calibration and must survive, only the start moves."""
    ent = OVERRIDES.get((cls, name))
    if ent is None:
        return None
    val = ent[0]
    if callable(val):
        if cur is None:
            raise SystemExit("%s: relative override needs the donor value" % name)
        val = val(unpack_field(ctype, count, cur))
    if ctype == "uint16_t":
        if len(val) != count:
            raise SystemExit("%s: %d values for a [%d] field" %
                             (name, len(val), count))
        return struct.pack("<%dH" % count, *val)
    if ctype == "chx_white_point_t":
        return struct.pack("<3f", *val)
    if ctype == "float":
        if count > 1:
            if len(val) != count:
                raise SystemExit("%s: %d values for a [%d] field" %
                                 (name, len(val), count))
            return struct.pack("<%df" % count, *val)
        return struct.pack("<f", val)
    if ctype == "uint32_t":
        return struct.pack("<I", val)
    raise SystemExit("no packer for overridden %s" % ctype)

def f32(v):
    """Shortest C float literal that round-trips to the same float32.

    `%.9g` is enough digits for any float32 but produces 1.92309999f for a
    value written as 1.9231, and -- worse -- bare `1f` for 1.0, which is not a
    C literal at all.  Try increasing precision until the bits match, then make
    sure the result still looks like a float."""
    bits = struct.pack("<f", v)
    for prec in range(1, 10):
        t = "%.*g" % (prec, v)
        if struct.pack("<f", float(t)) == bits:
            break
    if "." not in t and "e" not in t and "n" not in t and "i" not in t:
        t += ".0"
    return t + "f"

def fmt_field(ctype, count, raw):
    if ctype == "uint8_t":
        return "{ " + ", ".join("0x%02x" % x for x in raw) + " }"
    if ctype == "uint16_t":
        return "{ " + ", ".join("%d" % x
                                for x in struct.unpack_from("<%dH" % count, raw)) + " }"
    if ctype == "uint32_t":
        return "0x%08x" % struct.unpack_from("<I", raw)[0]
    if ctype == "float":
        if count > 1:
            v = struct.unpack_from("<%df" % count, raw)
            return "{ " + ", ".join(f32(x) for x in v) + " }"
        return f32(struct.unpack_from("<f", raw)[0])
    if ctype == "chx_white_point_t":
        r,g,b = struct.unpack_from("<3f", raw)
        return "{ %s, %s, %s }" % (f32(r), f32(g), f32(b))
    raise SystemExit("no formatter for %s" % ctype)

def emit_init(cls, data):
    L = []
    for kind, off, name, spec in layout(cls, len(data)):
        if kind == "gap":
            L.append("\t.%s = {" % name)
            L.append(fmt_bytes(data[off:off+spec]))
            L.append("\t},")
        else:
            ctype, count = spec
            w = CTYPE_SIZE[ctype] * count
            ov = override_bytes(cls, name, ctype, count, data[off:off+w])
            if ov is not None:
                L.append("\t/* talkman: %s */" % OVERRIDES[(cls, name)][1])
            L.append("\t.%s = %s," % (name, fmt_field(ctype, count, ov if ov is not None else data[off:off+w])))
    return "\n".join(L)

def rebuild(cls, data):
    """Reconstruct the payload from what emit_init would write -- the verify."""
    out = bytearray(len(data))
    for kind, off, name, spec in layout(cls, len(data)):
        if kind == "gap":
            out[off:off+spec] = data[off:off+spec]
            continue
        ctype, count = spec
        w = CTYPE_SIZE[ctype] * count
        ov = override_bytes(cls, name, ctype, count, data[off:off+w])
        out[off:off+w] = ov if ov is not None else data[off:off+w]
    return bytes(out)

# Carried over from mk-chromatix.py, which this replaces.  7.0 is CORRECT for
# this module -- Hill's measured flux at torch level 7 over level 2 is 7.030,
# and level 2 is our preflash (S153) -- and three earlier sessions tried to
# "fix" it.  The tripwire is here so a layout change fails loudly instead of
# shipping a silently wrong file.
def check_flux(cls, data):
    if cls != "chx_vfe_t":
        return
    den = struct.unpack_from("<f", data, 0xd4e8)[0]
    num = struct.unpack_from("<f", data, 0xd4ec)[0]  # struct-relative, S153
    if (den, num) != (1.0, 7.0):
        raise SystemExit(
            "gen-chromatix-c: AEC flux layout moved -- [0xd4e8]=%r [0xd4ec]=%r, "
            "expected 1.0 / 7.0.  Re-derive before shipping (live-csi-plan S153)." % (den, num))

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "verify"
    # cityman: data/chx_common.inc is the talkman port's file as shipped.  Its
    # lens rolloff mesh (struct 0x8bc8..0xa768) differs from the angler donor
    # in vendor/msft/cityman, and the rear colours were confirmed on the phone
    # with it, so regenerate selectively instead of overwriting it.
    only = sys.argv[2] if len(sys.argv) > 2 else ""
    seen, rc = {}, 0
    for n in NAMES:
        data = payload(os.path.join(DONOR, "libchromatix_imx377_%s.so" % n))
        cls  = CLASS[len(data)]
        check_flux(cls, data)
        key  = (len(data), data)
        owner = seen.setdefault(key, n)
        if mode == "verify":
            want = bytearray(data)
            novr = 0
            for kind, off, nm, spec in layout(cls, len(data)):
                if kind == "gap":
                    continue
                ctype, count = spec
                w = CTYPE_SIZE[ctype] * count
                ov = override_bytes(cls, nm, ctype, count, data[off:off+w])
                if ov is not None:
                    want[off:off+len(ov)] = ov
                    novr += 1
            ok = rebuild(cls, data) == bytes(want)
            rc |= 0 if ok else 1
            print("  %-14s %-16s %6d bytes  %s%s" % (n, cls, len(data),
                  "ok" if ok else "MISMATCH",
                  "  (%d field(s) deliberately overridden)" % novr if novr else ""))
        elif mode == "emit" and owner == n and n.startswith(only):
            inc = os.path.join(HERE, "data", "chx_%s.inc" % n)
            os.makedirs(os.path.dirname(inc), exist_ok=True)
            with open(inc, "w") as f:
                f.write("/* GENERATED by gen-chromatix-c.py -- do not edit by hand.\n"
                        " * Initialiser for %s.  Regenerate with `gen-chromatix-c.py emit`\n"
                        " * and check with `verify`. */\n" % cls)
                f.write(emit_init(cls, data) + "\n")
            print("  wrote data/chx_%s.inc (%s, %d bytes)" % (n, cls, len(data)))
    if mode == "emit":
        decls = [emit_struct_decl(c, s) for s, c in sorted(CLASS.items())]
        with open(os.path.join(HERE, "chromatix_structs.h"), "w") as f:
            f.write("/* GENERATED by gen-chromatix-c.py -- do not edit by hand. */\n"
                    "#ifndef CHROMATIX_STRUCTS_H\n#define CHROMATIX_STRUCTS_H\n"
                    '#include "chromatix_talkman.h"\n\n')
            f.write("\n\n".join(decls))
            f.write("\n\n#endif\n")
        print("  wrote chromatix_structs.h")
        # which .inc each name uses
        for n in NAMES:
            data = payload(os.path.join(DONOR, "libchromatix_imx377_%s.so" % n))
            print("     %-14s -> chx_%s.inc" % (n, seen[(len(data), data)]))
    return rc

sys.exit(main())
