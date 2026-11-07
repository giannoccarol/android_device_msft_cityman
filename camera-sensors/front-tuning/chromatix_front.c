/* The front camera chromatix: the rear's container (chromatix_lib.c) with the
 * two option macros of the front payloads (data/chx_front_*.inc).
 *
 * CITYMAN_FRONT_CCM(i, donor)
 *   VFE payloads, colour matrix entry i.  With CITYMAN_FRONT_CCM_HEADER (a
 *   header that gen-front-ccm.py writes into the build output from the
 *   private DCC) every entry becomes CITYMAN_FRONT_DCC_CCM; without it the
 *   donor's matrix stays.
 *
 * CITYMAN_FRONT_LIN(channel, donor start, donor knee)
 *   common payload, linearization knee inputs.  With CITYMAN_FRONT_BLACK_R,
 *   _GR, _GB and _B (12-bit) each ladder is shifted so it starts at that
 *   channel's black level, keeping the donor's segment widths: the rear's
 *   lin_shift(), done by the compiler.  Without them the donor's ladder stays.
 */
#ifdef CITYMAN_FRONT_CCM_HEADER
#include CITYMAN_FRONT_CCM_HEADER
#define CITYMAN_FRONT_CCM(i, ...) CITYMAN_FRONT_DCC_CCM
#else
#define CITYMAN_FRONT_CCM(i, ...) __VA_ARGS__
#endif

#ifdef CITYMAN_FRONT_BLACK_R
#define CITYMAN_FRONT_LIN(ch, start, v) \
	((v) - (start) + CITYMAN_FRONT_BLACK_##ch)
#else
#define CITYMAN_FRONT_LIN(ch, start, v) (v)
#endif

#include "chromatix_lib.c"

#ifdef CITYMAN_FRONT_BLACK_R
/* The VFE packs the knees as 12-bit fields: a ladder pushed past 4095 would
 * wrap and invert the curve.  CITYMAN_FRONT_LIN_SPAN (the widest donor
 * ladder) is written into chx_front_common.inc by the generator. */
#define CITYMAN_FRONT_BLACK_OK(x) ((x) >= 0 && (x) + CITYMAN_FRONT_LIN_SPAN <= 4095)
CHX_STATIC_ASSERT(CITYMAN_FRONT_BLACK_OK(CITYMAN_FRONT_BLACK_R) &&
                  CITYMAN_FRONT_BLACK_OK(CITYMAN_FRONT_BLACK_GR) &&
                  CITYMAN_FRONT_BLACK_OK(CITYMAN_FRONT_BLACK_GB) &&
                  CITYMAN_FRONT_BLACK_OK(CITYMAN_FRONT_BLACK_B),
                  front_black_level_in_range);
#endif
