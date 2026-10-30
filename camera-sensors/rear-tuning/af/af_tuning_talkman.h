/* AF search tuning for talkman (Lumia 950, IMX230 "Hill").
 *
 * WHAT THIS LIBRARY IS
 * --------------------
 * `af_actuator_init()` dlopens `libactuator_<name>_<mode>.so` once per mode --
 * camera and camcorder -- and a failure in either fails the whole session, so
 * both must exist.  Like a chromatix library it is a data container: the only
 * export is
 *
 *     void *talkman_<mode>_af_algo_open_lib(void);
 *
 * three instructions returning `.data + 8`.  `.data` holds a self-pointer, one
 * zero word, then a 4992-byte AF *search* tuning struct that begins with a
 * version pair and the ASCII vendor name.
 *
 * WHERE THE VALUES COME FROM, AND WHAT IS STILL OWED
 * ---------------------------------------------------
 * The donor is the ROM's own `libactuator_lc898212xd_*`, from a Moto clark --
 * the same IMX230 with near-identical optics, so its *search behaviour* is the
 * right starting point.  This is AF search tuning, not optics: it decides how
 * far and how fast the loop hunts, not where the lens focuses.
 *
 * What is genuinely ours is every field expressed in actuator steps, because
 * this module's step table is its own: the NVM-calibrated range is 900..1436,
 * 268 steps of 2 codes, against the donor's 324 (S38-S39).  Those fields are
 * listed as OVERRIDES in gen-af-c.py with the scale factor applied, and its
 * `verify` prints them.
 *
 * ⚠ Do NOT scale the scan-step tables at 0x172.  `af_single_get_scan_step`
 * computes `steps = step_index_per_um * table_value`, and the daemon logs
 * `1 step pre-scan 22, finescan 10 step_per_um 1`, so those eight tables are in
 * MICROMETRES and are already lens-independent.  An earlier note calling them
 * "~20% too large in step units" was wrong about the units.
 *
 * ⚠ Do NOT invert CAF/TAF far_end and near_end.  AF's focus_pos space is the
 * inverse of the actuator's step space for every lens -- the AF port decodes a
 * move as `cur_pos - steps` while the sensor module decodes it as
 * `cur_pos + sign(dir)*steps` -- so the donor's near 0 / far 324 is already
 * correct once scaled.  Inverting them made AF emit `dir 0, steps -268`, which
 * the sensor module clamped to a destination of 0 and no move at all (S38).
 *
 * DECODE STATUS
 * -------------
 * 42 fields plus the scan-step tables are named below; the rest sits in
 * `_gap_*` arrays labelled with their own offset.  The map came from
 * `af_set_debug_print_af_tuning_parms` and `af_util_is_low_light` in
 * libmmcamera2_q3a_core.so, with every offset checked against what the daemon
 * printed for the donor.
 */
#ifndef AF_TUNING_TALKMAN_H
#define AF_TUNING_TALKMAN_H

#include <stdint.h>

#define AF_STATIC_ASSERT(c, name) typedef char af_assert_##name[(c) ? 1 : -1]
#define AF_CHECK_OFF(type, field, off, tag) \
	AF_STATIC_ASSERT(__builtin_offsetof(type, field) == (off), tag)

#endif /* AF_TUNING_TALKMAN_H */
