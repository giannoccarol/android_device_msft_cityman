#!/usr/bin/env python3
"""Compare built libchromatix_imx230_*.so payloads against the donor.

The point is not that the bytes came from the donor -- of course they did, the
tuning has not been re-derived yet.  The point is that converting the container
to C changed NOTHING except the fields this port deliberately overrode, and
that those are exactly the ones gen-chromatix-c.py declares.

    verify-built.py <dir of built .so>
"""
import os, re, struct, subprocess, sys

DONOR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
        "../../../../../../../vendor/msft/talkman/proprietary/vendor/lib"))
NAMES = ["common","snapshot","default_video","liveshot","postproc",
         "cpp_preview","cpp_snapshot","cpp_video","cpp_uhd_video","cpp_liveshot"]
# struct offset -> packed bytes, from gen-chromatix-c.py's OVERRIDES
OVR_VFE = {0x6eac: struct.pack("<3f", 1.9231, 1.0, 1.6750),
           0x6eb8: struct.pack("<3f", 1.9231, 1.0, 1.6750)}

def data(path):
    out = subprocess.run(["readelf","-S",path],capture_output=True,text=True).stdout
    for l in out.splitlines():
        if re.search(r'\.data\s+PROGBITS', l):
            p=l.split(); k=p.index('PROGBITS')
            return open(path,'rb').read()[int(p[k+2],16):][:int(p[k+3],16)]
    raise SystemExit("no .data in %s" % path)

def main(d):
    rc = 0
    for n in NAMES:
        ours  = data(os.path.join(d, "libchromatix_imx230_%s.so" % n))[4:]
        donor = bytearray(data(os.path.join(DONOR, "libchromatix_imx377_%s.so" % n))[4:])
        ovr = OVR_VFE if len(donor) == 67064 else {}
        for off, b in ovr.items():
            donor[off:off+len(b)] = b
        diff = [i for i,(a,b) in enumerate(zip(ours, bytes(donor))) if a != b]
        ok = len(ours) == len(donor) and not diff
        rc |= 0 if ok else 1
        print("  %-14s %6d bytes  %s%s" % (n, len(ours),
              "identical to donor" if ok else "MISMATCH (%d bytes)" % len(diff),
              " + %d declared override(s)" % len(ovr) if ovr else ""))
        if diff:
            for i in diff[:6]:
                print("       0x%04x  ours %02x donor %02x" % (i, ours[i], donor[i]))
    return rc

sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
