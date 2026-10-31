/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CITYMAN_IMX230_PROFILE_H
#define CITYMAN_IMX230_PROFILE_H

#ifdef CITYMAN_CAMERA_NUBIA24
#define PROFILE_NAME "nubia24"
#define PROFILE_WIDTH 5344u
#define PROFILE_HEIGHT 4016u
#define PROFILE_FRAME_LINES 4150u
#define PROFILE_LINE_PIXELS 6024u
#define PROFILE_MCLK 24000000u
#define PROFILE_VT_CLOCK 600000000u
#define PROFILE_OUTPUT_CLOCK 568000000u
#define PROFILE_MIN_FPS 7.5f
#define PROFILE_MAX_FPS 24.0f
#define PROFILE_BINNING 1
#define PROFILE_SETTLE 27
#define PROFILE_CIDS 3
#define PROFILE_MODE nubia24_mode0
#elif defined(CITYMAN_CAMERA_LUMIA20)
#define PROFILE_NAME "lumia20"
#define PROFILE_WIDTH 5344u
#define PROFILE_HEIGHT 3744u
/* The DCC has no frame length (0x0340/41). 3744 equals the output height and
 * leaves no vertical blanking; 3952 adds 208 lines (nubia24 keeps 134) and
 * gives exactly 20 fps: 476.16 MHz / (6024 * 3952). */
#define PROFILE_FRAME_LINES 3952u
#define PROFILE_LINE_PIXELS 6024u
#define PROFILE_MCLK 9600000u
#define PROFILE_VT_CLOCK 476160000u
#define PROFILE_OUTPUT_CLOCK 476160000u
#define PROFILE_MIN_FPS 5.0f
#define PROFILE_MAX_FPS 20.0f
#define PROFILE_BINNING 1
#define PROFILE_SETTLE 22
#define PROFILE_CIDS 1
#define PROFILE_MODE lumia20_mode0
#else
#define PROFILE_NAME "smia9"
#define PROFILE_WIDTH 2496u
#define PROFILE_HEIGHT 1872u
#define PROFILE_FRAME_LINES 2550u
#define PROFILE_LINE_PIXELS 6024u
#define PROFILE_MCLK 9600000u
#define PROFILE_VT_CLOCK 460800000u
#define PROFILE_OUTPUT_CLOCK 460800000u
#define PROFILE_MIN_FPS 15.0f
#define PROFILE_MAX_FPS 29.99766f
#define PROFILE_BINNING 2
#define PROFILE_SETTLE 22
#define PROFILE_CIDS 1
#define PROFILE_MODE smia_mode4
#endif

#ifdef CITYMAN_CAMERA_LUMIA20_BINNED
#ifndef CITYMAN_CAMERA_LUMIA20
#error "CITYMAN_CAMERA_LUMIA20_BINNED needs the lumia20 profile"
#endif
/*
 * Second mode, opt-in (LUMIA20-BINNED.md): 2x2 binning of the full lumia20
 * window, so the same field of view, PLL, link rate and line length.
 * 476.16 MHz / (6024 * 2634) = 30.009 fps: the daemon only picks a mode for
 * a 30 fps request when its maximum fps is at least 30.0.
 */
#define PROFILE_BINNED_WIDTH 2672u
#define PROFILE_BINNED_HEIGHT 1872u
#define PROFILE_BINNED_FRAME_LINES 2634u
#define PROFILE_BINNED_MIN_FPS 7.5f
#define PROFILE_BINNED_MAX_FPS 30.0f
#define PROFILE_MODES 2
#define PROFILE_MIN_FRAME_LINES PROFILE_BINNED_FRAME_LINES
#else
#define PROFILE_MODES 1
#define PROFILE_MIN_FRAME_LINES PROFILE_FRAME_LINES
#endif

#endif
