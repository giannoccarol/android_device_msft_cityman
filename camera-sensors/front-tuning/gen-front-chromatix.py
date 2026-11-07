#!/usr/bin/env python3
"""Emit the front camera's chromatix from the imx179_liteon donor.

    ./gen-front-chromatix.py emit      write data/chx_front_*.inc
    ./gen-front-chromatix.py verify    prove they reproduce the donor except
                                       for the overrides below
    ./gen-front-chromatix.py verify-built <dir> [<0A214000.dcc>]
                                       check built libchromatix_vs6955_*.so

The front VS6955 uses the vendor's imx179_liteon chromatix (angler blobs).
The three VFE/3A payloads it loads are rebuilt here, with brighter
auto-exposure targets, and so is the common payload, for the black level.
Every other byte is the donor's, and the other front chromatix stay the
vendor blobs.  The struct layout, the decoded fields and the C container are
the rear tuning's (../rear-tuning/chromatix, from hraj9258's talkman port), so
this reuses that generator's functions.

Two kinds of field are emitted as macros instead of plain values, so the
build can replace them without a second copy of the payload
(chromatix_front.c):

  .ccm_N             CITYMAN_FRONT_CCM(N, {donor})       colour matrix
  .lin_knee_in_tT_C  { CITYMAN_FRONT_LIN(C, start, v), ... }  black level

With no option set both expand to the donor's value, so `verify` (which
checks the donor reading) and `verify-built` on the imx179 libraries are the
same check.  The DCC matrices never pass through this script's output: the
build writes them to a generated header (gen-front-ccm.py).

CHX_DONOR points at the donor libraries when this tree is not inside the
Android source tree.
"""
import importlib.util, os, re, struct, sys

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "rear_gen", os.path.join(HERE, "../rear-tuning/chromatix/gen-chromatix-c.py"))
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

DONOR = os.environ.get("CHX_DONOR") or os.path.normpath(
    os.path.join(HERE, "../../../../../vendor/msft/cityman/proprietary/vendor/lib"))

# sensor_lib_vs6955.c names: preview (also used for snapshot), video_binning
# and liveshot, all chx_vfe_t, and common (chx_common_t).
VFE_NAMES = ["preview", "video_binning", "liveshot"]
NAMES = VFE_NAMES + ["common"]
CCM_SETS = ["imx179", "swarc", "hwqc"]

# Brighter AEC targets (offsets and the evidence for them: FIELDS in the rear
# generator).  The donor's are default 51, outdoor 50 and lowlight 45 from
# lux index 399 to 429; on this module the front preview converged on target
# 51 at ISO 1889, 1/15 s indoors and still looked dark.  Raised by about a
# fifth; the lowlight target less, since the gain is already high there.
gen.OVERRIDES = {
    ("chx_vfe_t", "aec_luma_target"):
      ((62, 58),
       "front: default (indoor) luma target 51 -> 62, outdoor 50 -> 58; the "
       "imx179_liteon targets left indoor previews dark on the VS6955"),
    ("chx_vfe_t", "aec_lowlight_luma_target"):
      (50,
       "front: lowlight luma target 45 -> 50, a smaller step because the "
       "gain is already high in dim rooms"),
}

CCM_RE = re.compile(r"^\t\.ccm_(\d) = (\{ .* \}),$")
LIN_RE = re.compile(r"^\t\.lin_knee_in_t(\d)_(r|gr|gb|b) = \{ ([0-9, ]+) \},$")
CCM_OFF = [0x40a8 + 0x2c * i for i in range(8)]


def macro_fields(cls, text):
    """Wrap the CCM and black level fields of emitted text in macros."""
    out, nccm, nlin = [], 0, 0
    for line in text.split("\n"):
        m = CCM_RE.match(line)
        if m and cls == "chx_vfe_t":
            line = "\t.ccm_%s = CITYMAN_FRONT_CCM(%s, %s)," % (m.group(1), m.group(1), m.group(2))
            nccm += 1
        m = LIN_RE.match(line)
        if m and cls == "chx_common_t":
            vals = [int(v) for v in m.group(3).split(",")]
            ch = m.group(2).upper()
            line = "\t.lin_knee_in_t%s_%s = { %s }," % (
                m.group(1), m.group(2),
                ", ".join("CITYMAN_FRONT_LIN(%s, %d, %d)" % (ch, vals[0], v) for v in vals))
            nlin += 1
        out.append(line)
    want = {"chx_vfe_t": (8, 0), "chx_common_t": (0, gen.LIN_GROUPS)}[cls]
    if (nccm, nlin) != want:
        raise SystemExit("%s: wrapped %d ccm / %d lin fields, expected %d / %d"
                         % (cls, nccm, nlin, want[0], want[1]))
    return "\n".join(out)


def lin_span(data):
    """Largest knee-ladder width (top - start) in the donor's common payload:
    the black level start may not exceed 4095 minus this."""
    span = 0
    for off, name, ctype, count in gen.LIN_KNEE_FIELDS:
        v = struct.unpack_from("<8H", data, off)
        span = max(span, v[-1] - v[0])
    return span


def donor(n):
    data = gen.payload(os.path.join(DONOR, "libchromatix_imx179_liteon_%s.so" % n))
    cls = gen.CLASS[len(data)]
    if cls != ("chx_common_t" if n == "common" else "chx_vfe_t"):
        raise SystemExit("%s: unexpected struct class %s" % (n, cls))
    return cls, data


def expected(cls, data):
    """The donor payload with the declared overrides applied."""
    want = bytearray(data)
    novr = 0
    for kind, off, nm, spec_ in gen.layout(cls, len(data)):
        if kind == "gap":
            continue
        ctype, count = spec_
        w = gen.CTYPE_SIZE[ctype] * count
        ov = gen.override_bytes(cls, nm, ctype, count, data[off:off + w])
        if ov is not None:
            want[off:off + len(ov)] = ov
            novr += 1
    return bytes(want), novr


def emit():
    for n in NAMES:
        cls, data = donor(n)
        inc = os.path.join(HERE, "data", "chx_front_%s.inc" % n)
        with open(inc, "w") as f:
            f.write("/* GENERATED by gen-front-chromatix.py -- do not edit by hand.\n"
                    " * Initialiser for %s from libchromatix_imx179_liteon_%s.so.\n"
                    " * Regenerate with `gen-front-chromatix.py emit` and check with\n"
                    " * `verify`. */\n" % (cls, n))
            if cls == "chx_common_t":
                f.write("#define CITYMAN_FRONT_LIN_SPAN %d\n" % lin_span(data))
            f.write(macro_fields(cls, gen.emit_init(cls, data)) + "\n")
        print("  wrote data/chx_front_%s.inc" % n)
    return 0


def verify():
    rc = 0
    for n in NAMES:
        cls, data = donor(n)
        want, novr = expected(cls, data)
        ok = gen.rebuild(cls, data) == want
        diff = sum(1 for i in range(len(data)) if data[i] != want[i])
        rc |= 0 if ok else 1
        print("  %-14s %s  %d byte(s) differ from the donor, %d field(s) overridden"
              % (n, "ok" if ok else "MISMATCH", diff, novr))
    return rc


def dcc_candidates(dcc, name):
    """float32 bytes of every matrix the build can put in a `name` library."""
    spec_ = importlib.util.spec_from_file_location("ccm", os.path.join(HERE, "gen-front-ccm.py"))
    ccm = importlib.util.module_from_spec(spec_)
    spec_.loader.exec_module(ccm)
    data = ccm.load(dcc)
    return {idx: struct.pack("<9f", *ccm.matrix(data, name, idx))
            for idx in ["mean"] + [str(i) for i in range(ccm.COUNT)]}


def verify_built(d, dcc):
    """Built libraries against the donor: only the declared fields may differ.
    Prints no matrix values, so the output can go into a report."""
    rc, seen = 0, 0
    for s in CCM_SETS:
        cands = dcc_candidates(dcc, s) if dcc and s != "imx179" else None
        for n in VFE_NAMES:
            path = os.path.join(d, "libchromatix_vs6955_%s_%s.so" % (s, n))
            if not os.path.exists(path):
                continue
            seen += 1
            cls, data = donor(n)
            want, _ = expected(cls, data)
            ours = gen.payload(path)
            note = "CCM = donor"
            if s != "imx179":
                m = ours[CCM_OFF[0]:CCM_OFF[0] + 36]
                same = all(ours[o:o + 36] == m for o in CCM_OFF)
                want = bytearray(want)
                for o in CCM_OFF:
                    want[o:o + 36] = m
                want = bytes(want)
                hit = [k for k, v in (cands or {}).items() if v == m]
                if not same:
                    note = "CCM entries DIFFER"
                elif cands is None:
                    note = "same CCM in all 8 entries, not checked against the DCC"
                elif hit:
                    note = "CCM = DCC %s %s in all 8 entries" % (s, hit[0])
                else:
                    note = "CCM is NOT a DCC matrix"
                if not same or (cands is not None and not hit):
                    rc = 1
            ok = len(ours) == len(want) and ours == want
            rc |= 0 if ok else 1
            print("  %-6s %-14s %s, %s" % (s, n, "rest identical to donor + AEC overrides"
                                           if ok else "MISMATCH", note))
    path = os.path.join(d, "libchromatix_vs6955_common.so")
    if os.path.exists(path):
        seen += 1
        cls, data = donor("common")
        ours = gen.payload(path)
        want = bytearray(data)
        starts, ok = set(), len(ours) == len(data)
        for off, name, ctype, count in gen.LIN_KNEE_FIELDS:
            a = struct.unpack_from("<8H", ours, off)
            b = struct.unpack_from("<8H", data, off)
            shift = a[0] - b[0]
            ok &= all(x - y == shift for x, y in zip(a, b))
            starts.add((name.rsplit("_", 1)[1], a[0]))
            want[off:off + 16] = ours[off:off + 16]
        ok &= ours == bytes(want)
        rc |= 0 if ok else 1
        print("  common %s, ladder starts %s" % (
            "widths kept, rest identical to donor" if ok else "MISMATCH",
            " ".join("%s=%d" % c for c in sorted(starts, key=lambda c: "r gr gb b".split().index(c[0])))))
    if not seen:
        print("  no libchromatix_vs6955_*.so in %s" % d)
        return 1
    return rc


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "verify"
    if mode == "emit":
        return emit()
    if mode == "verify-built":
        if len(sys.argv) < 3:
            raise SystemExit("verify-built <dir of built .so> [<0A214000.dcc>]")
        return verify_built(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    return verify()


if __name__ == "__main__":
    sys.exit(main())
