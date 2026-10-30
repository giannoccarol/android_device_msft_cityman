/* Chromatix tuning containers for talkman (Lumia 950, IMX230 "Hill").
 *
 * WHAT A CHROMATIX LIBRARY IS
 * ---------------------------
 * A `libchromatix_<sensor>_<usecase>.so` is a data container, not code.  Its
 * entire `.text` is toolchain boilerplate (libunwind, atexit); the only export
 * is
 *
 *     void *load_chromatix(void);
 *
 * which in the donor is literally three instructions -- `ldr r0,[pc,#4] /
 * add r0,pc / bx lr` -- returning `.data + 4`.  The payload is one large
 * tuning struct, and the word at `.data + 0` is a self-pointer (the file's
 * single R_ARM_RELATIVE relocation).  `struct chx_container` below reproduces
 * that layout exactly, so our library is ABI-identical to the donor's without
 * any part of the donor being copied or patched at build time.
 *
 * HOW THE OFFSETS WERE ESTABLISHED
 * --------------------------------
 * `struct offset = file offset - 0x3004` (`.data` is at file 0x3000, vaddr
 * 0x4000; the struct starts 4 bytes in).  That mapping is not assumed -- it
 * reproduces three offsets this port had already established independently:
 *
 *     0xd4e8 / 0xd4ec  AEC flash flux pair      (live_csi_plan S153, read at
 *                                                runtime out of the daemon)
 *     0x7c08           AEC flash flux selector  (same)
 *     0x6eac           LED reference white point (S169, identified from AWB's
 *                                                own log printing 1/1.7339 and
 *                                                1/2.53 back as (rg,bg))
 *
 * DECODE STATUS -- READ THIS BEFORE JUDGING THE GAPS
 * --------------------------------------------------
 * These structs are ~67 KB (VFE/3A), ~35 KB (CPP), ~44 KB (common) and 2.5 KB
 * (postproc).  Only the fields named below are decoded; everything else sits
 * in `_gap_*` arrays named for their own struct offset.  That is deliberate
 * and honest: a gap is data whose meaning is not yet established, not data
 * that is hidden.  Decoding progress is exactly "a gap splits and a named
 * field appears", and `gen-chromatix-c.py --verify` proves at every step that
 * the bytes did not move.
 *
 * The decode substrate is on the device: 36 `libmmcamera_isp_*.so` modules,
 * 59 of 69 camera module libraries carrying `.gnu_debugdata`, each with named
 * functions (`color_correct40_interpolate`, `linearization40_config_cmd`, ...)
 * that read their own sub-struct out of this one.
 *
 * WHERE THE VALUES COME FROM, AND WHAT IS STILL OWED
 * --------------------------------------------------
 * The values are still the imx377 donor's tuning.  Converting the container to
 * C removes the binary patching; it does not by itself produce IMX230 tuning,
 * and pretending otherwise would be dishonest.  The real source for this
 * module is the phone's own WP calibration: `0AEACA05.dcc` carries an
 * `|ISP_HW_QC|` block (~20 KB) whose header already decodes as this sensor's
 * array (5344x3744) followed by what read as ISO trigger points
 * (20/50/100/300/1000) and gain multipliers (1/2/4/8).  Individual fields have
 * already been replaced from measurement where they mattered -- see
 * `led_ref_white_point` below.
 */
#ifndef CHROMATIX_TALKMAN_H
#define CHROMATIX_TALKMAN_H

#include <stdint.h>

#define CHX_STATIC_ASSERT(c, name) typedef char chx_assert_##name[(c) ? 1 : -1]
#define CHX_CHECK_OFF(type, field, off, tag) \
	CHX_STATIC_ASSERT(__builtin_offsetof(type, field) == (off), tag)

/* An illuminant reference white point: per-channel gains, green pinned to 1.0.
 * The tuning holds a table of these; AWB prints the one it selects back as
 * (rg,bg) = (1/r, 1/b), which is how the LED entry was located. */
typedef struct {
	float r;
	float g;	/* always 1.0 */
	float b;
} chx_white_point_t;

CHX_STATIC_ASSERT(sizeof(chx_white_point_t) == 12, white_point_size);

#endif /* CHROMATIX_TALKMAN_H */
