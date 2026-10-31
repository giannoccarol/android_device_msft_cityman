/* SPDX-License-Identifier: GPL-2.0-only */
/* Freestanding ARM32 behavioral tests; run under qemu-arm, never on a phone. */
#include "angler_sensor_abi.h"
#include "profile.h"
#define REQUIRE(c) do { if (!(c)) return __LINE__; } while (0)

static uint32_t read_word(struct msm_camera_i2c_reg_array *r, unsigned int n, uint16_t address)
{
    unsigned int i;
    uint32_t high = 0, low = 0;
    for (i = 0; i < n; i++) {
        if (r[i].reg_addr == address) high = r[i].reg_data;
        if (r[i].reg_addr == address + 1) low = r[i].reg_data;
    }
    return high * 256 + low;
}

/* A word from a resolution table, else from the init tables (lumia20 seeds
 * the frame length at init when it has a single mode). */
static uint32_t mode_word(struct angler_sensor_descriptor *d, unsigned int mode, uint16_t address)
{
    struct msm_camera_i2c_reg_setting *s = &d->resolutions.entries[mode];
    unsigned int i;
    uint32_t value = read_word(s->reg_setting, s->size, address);
    for (i = d->init.count; !value && i > 0; i--)
        value = read_word(d->init.entries[i - 1].reg_setting, d->init.entries[i - 1].size, address);
    return value;
}

static int read_byte(struct msm_camera_i2c_reg_setting *s, uint16_t address)
{
    unsigned int i;
    for (i = 0; i < s->size; i++)
        if (s->reg_setting[i].reg_addr == address) return s->reg_setting[i].reg_data;
    return -1;
}

/* Every write of a in [first, last] has the same value in b, and vice versa. */
static int same_writes(struct msm_camera_i2c_reg_setting *a, struct msm_camera_i2c_reg_setting *b,
                       uint16_t first, uint16_t last)
{
    unsigned int i;
    for (i = 0; i < a->size; i++) {
        uint16_t r = a->reg_setting[i].reg_addr;
        if (r >= first && r <= last && read_byte(a, r) != read_byte(b, r)) return 0;
    }
    for (i = 0; i < b->size; i++) {
        uint16_t r = b->reg_setting[i].reg_addr;
        if (r >= first && r <= last && read_byte(a, r) != read_byte(b, r)) return 0;
    }
    return 1;
}

static int check_mode(struct angler_sensor_descriptor *d, unsigned int m)
{
    struct angler_mode *mode = &d->modes.entries[m];
    struct msm_camera_i2c_reg_setting out = d->resolutions.entries[m];
    uint32_t x_range, y_range, binning, frame;
    double fps, line_bits, link_bits;
    REQUIRE(out.size > 12 && out.addr_type == MSM_CAMERA_I2C_WORD_ADDR);
    REQUIRE(read_word(out.reg_setting, out.size, 0x034c) == mode->width);
    REQUIRE(read_word(out.reg_setting, out.size, 0x034e) == mode->height);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0112) == 0x0a0a);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0342) == mode->line_length);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0342) == PROFILE_LINE_PIXELS);
    frame = mode_word(d, m, 0x0340);
    REQUIRE(frame == mode->frame_length && frame >= mode->height + 8u);
    REQUIRE(PROFILE_MCLK / read_word(out.reg_setting, out.size, 0x0304) *
            read_word(out.reg_setting, out.size, 0x0306) /
            (read_word(out.reg_setting, out.size, 0x0302) *
             read_word(out.reg_setting, out.size, 0x0300)) * 4 == mode->vt_pixel_clock);
    REQUIRE(PROFILE_MCLK / read_word(out.reg_setting, out.size, 0x030c) *
            read_word(out.reg_setting, out.size, 0x030e) /
            (read_word(out.reg_setting, out.size, 0x030a) *
             read_word(out.reg_setting, out.size, 0x0308)) * 4 == mode->output_pixel_clock);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0820) == mode->output_pixel_clock / 100000);
    /* Readout window, SMIA++ binning and the output/crop sizes agree. */
    x_range = read_word(out.reg_setting, out.size, 0x0348) - read_word(out.reg_setting, out.size, 0x0344) + 1;
    y_range = read_word(out.reg_setting, out.size, 0x034a) - read_word(out.reg_setting, out.size, 0x0346) + 1;
    binning = mode->binning_factor;
    REQUIRE(binning == 1 || binning == 2);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0900) >> 8 == (binning == 2));
    REQUIRE(read_word(out.reg_setting, out.size, 0x0900) == (binning == 2 ? 0x0122 : 0x0011));
    REQUIRE(x_range == mode->width * binning && y_range == mode->height * binning);
    REQUIRE(read_word(out.reg_setting, out.size, 0x040c) == mode->width);
    REQUIRE(read_word(out.reg_setting, out.size, 0x040e) == mode->height);
#ifndef CITYMAN_CAMERA_NUBIA24
    /* Vendor mirrors of the Lumia/SMIA tables: analog width, output height. */
    REQUIRE(read_word(out.reg_setting, out.size, 0x3a23) == x_range);
    REQUIRE(read_word(out.reg_setting, out.size, 0x3a25) == mode->height);
    REQUIRE(read_byte(&out, 0x3a38) == (binning == 2));
#endif
    /* Frame timing: the advertised maximum fps is what the clocks give. */
    fps = (double)mode->vt_pixel_clock / ((double)mode->line_length * frame);
    REQUIRE(fps >= mode->maximum_fps * 0.999 && fps <= mode->maximum_fps * 1.001);
    REQUIRE(mode->minimum_fps > 0.0f && mode->minimum_fps <= mode->maximum_fps);
    REQUIRE(frame * (mode->maximum_fps / mode->minimum_fps) < 65535.0f);
    /* MIPI budget: RAW10 bits of one line fit the four lanes in one line time. */
    line_bits = mode->width * 10.0;
    link_bits = read_word(out.reg_setting, out.size, 0x0820) * 1e6 * mode->line_length / mode->vt_pixel_clock;
    REQUIRE(line_bits < link_bits * 0.95);
    REQUIRE(d->csi.entries[m].phy.settle_cnt == PROFILE_SETTLE);
    REQUIRE(d->csi.entries[m].csid.lut_params.num_cid == PROFILE_CIDS);
    REQUIRE(d->csi.entries[m].csid.lane_assign == 0x0423 && d->csi.entries[m].csid.lane_cnt == 4);
    REQUIRE(d->tuning.entries[m].names[0] && d->tuning.entries[m].names[11]);
    return 0;
}

int run_tests(void)
{
    struct angler_sensor_descriptor *d = sensor_open_lib();
    struct angler_exposure e;
    struct { uint32_t before; struct msm_camera_i2c_reg_array r[8]; uint32_t after; } buffer;
    struct msm_camera_i2c_reg_setting out = {.reg_setting = buffer.r};
    union { uint32_t bits; float f; } invalid = {.bits = 0x7fc00000};
    uint32_t gains[] = {1, 2, 4, 8, 16};
    uint32_t expected[] = {0, 256, 384, 448, 448};
    unsigned int i, j;
    int smia_idle_seen = 0, failed;
    REQUIRE(d == &cityman_imx230_descriptor);
    REQUIRE(d->modes.count == PROFILE_MODES && d->resolutions.count == PROFILE_MODES &&
            d->csi.count == PROFILE_MODES && d->crops.count == PROFILE_MODES &&
            d->tuning.count == PROFILE_MODES);
    REQUIRE(d->slave.slave_addr == 0x20 && d->slave.sensor_id_info.sensor_id == 0x0230);
    REQUIRE(d->start.reg_setting[0].reg_data == 1 && d->stop.reg_setting[0].reg_data == 0);
    REQUIRE(d->output.raw_depth == 1 && d->streams.entries[0].cid[0].decode_format == CSI_DECODE_10BIT);
    REQUIRE(d->exposure.calculate(1.0f, 1, 0) < 0);
    REQUIRE(d->exposure.calculate(0.0f, 1, &e) < 0);
    REQUIRE(d->exposure.calculate(-1.0f, 1, &e) < 0);
    REQUIRE(d->exposure.calculate(invalid.f, 1, &e) < 0);
    invalid.bits = 0x7f800000;
    REQUIRE(d->exposure.calculate(invalid.f, 1, &e) < 0);
    for (i = 0; i < COUNT(gains); i++) {
        REQUIRE(d->exposure.calculate((float)gains[i], 1200, &e) == 0);
        REQUIRE(e.register_gain == expected[i] && e.lines == 1200);
        REQUIRE(e.sensor_real_gain * e.isp_digital_gain == (float)gains[i]);
    }
    REQUIRE(d->exposure.calculate(1.0f, 0, &e) == 0 && e.lines == 4);
    REQUIRE(d->exposure.calculate(1.0f, 0xffffffffu, &e) == 0 && e.lines == 65527);
    REQUIRE(d->exposure.fill(0, 0, 0, 0, 0, 0, 0) < 0);
    out.reg_setting = 0;
    REQUIRE(d->exposure.fill(0, 0, 0, 0, 0, 0, &out) < 0);
    out.reg_setting = buffer.r;
    buffer.before = 0x12345678; buffer.after = 0x87654321;
    REQUIRE(d->exposure.fill(0xffffffffu, 0x100, 0xffffffffu, 0xffffffffu, 0, 0, &out) == 0);
    REQUIRE(buffer.before == 0x12345678 && buffer.after == 0x87654321);
    REQUIRE(out.size == 8 && out.data_type == MSM_CAMERA_I2C_BYTE_DATA);
    REQUIRE(buffer.r[0].reg_addr == 0x0104 && buffer.r[0].reg_data == 1);
    REQUIRE(buffer.r[7].reg_addr == 0x0104 && buffer.r[7].reg_data == 0);
    REQUIRE(read_word(buffer.r, out.size, 0x0340) == 65535);
    REQUIRE(read_word(buffer.r, out.size, 0x0202) == 65527);
    REQUIRE(read_word(buffer.r, out.size, 0x0204) == 448);
    REQUIRE(d->exposure.fill(0, 0x100, 3000, 2550, 0, 0, &out) == 0);
    REQUIRE(read_word(buffer.r, out.size, 0x0340) == (PROFILE_MIN_FRAME_LINES > 3008 ? PROFILE_MIN_FRAME_LINES : 3008));
    /* Every mode's own frame length passes through unchanged. */
    for (i = 0; i < d->modes.count; i++) {
        REQUIRE(d->exposure.fill(0, 0x100, 1000, d->modes.entries[i].frame_length, 0, 0, &out) == 0);
        REQUIRE(read_word(buffer.r, out.size, 0x0340) == d->modes.entries[i].frame_length);
    }
    REQUIRE(d->exposure.fill(0, 0x100, 0, 0, 0, 1, &out) < 0);
    for (i = 0; i < d->init.count; i++)
        for (j = 0; j < d->init.entries[i].size; j++) {
            uint16_t a = d->init.entries[i].reg_setting[j].reg_addr;
            REQUIRE(a != 0 && a != 0x0100 && a != 0x0107);
        }
    /* EXTCLK must match MCLK in the big init table (smia_idle / nubia24_init). */
    for (i = 0; i < d->init.count; i++) {
        out = d->init.entries[i];
        if (out.size > 12 && out.data_type == MSM_CAMERA_I2C_BYTE_DATA && read_byte(&out, 0x0136) >= 0) {
            REQUIRE(read_word(out.reg_setting, out.size, 0x0136) == (PROFILE_MCLK / 1000 * 256) / 1000);
            smia_idle_seen = 1;
        }
    }
    REQUIRE(smia_idle_seen);
    for (i = 0; i < d->modes.count; i++)
        if ((failed = check_mode(d, i)) != 0) return failed;
    /* Mode 0 is the full readout: the active array and the daemon fallback. */
    REQUIRE(d->modes.entries[0].width == PROFILE_WIDTH && d->modes.entries[0].height == PROFILE_HEIGHT);
    REQUIRE(d->modes.entries[0].vt_pixel_clock == PROFILE_VT_CLOCK);
    REQUIRE(d->modes.entries[0].output_pixel_clock == PROFILE_OUTPUT_CLOCK);
    REQUIRE(d->active_array.width == PROFILE_WIDTH && d->active_array.height == PROFILE_HEIGHT);
    for (i = 1; i < d->modes.count; i++) {
        struct angler_mode *m = &d->modes.entries[i];
        REQUIRE(m->width * m->height < (uint32_t)PROFILE_WIDTH * PROFILE_HEIGHT);
        /* Same field of view as the active array, so crop maps by scaling. */
        REQUIRE(m->width * m->binning_factor == PROFILE_WIDTH && m->height * m->binning_factor == PROFILE_HEIGHT);
        /* Same PLL, link rate, window and CSI: the CSIPHY settle still holds. */
        REQUIRE(same_writes(&d->resolutions.entries[0], &d->resolutions.entries[i], 0x0111, 0x0114));
        REQUIRE(same_writes(&d->resolutions.entries[0], &d->resolutions.entries[i], 0x0300, 0x0310));
        REQUIRE(same_writes(&d->resolutions.entries[0], &d->resolutions.entries[i], 0x0344, 0x034b));
        REQUIRE(same_writes(&d->resolutions.entries[0], &d->resolutions.entries[i], 0x0820, 0x0823));
        REQUIRE(m->vt_pixel_clock == PROFILE_VT_CLOCK && m->output_pixel_clock == PROFILE_OUTPUT_CLOCK);
        REQUIRE(d->resolutions.entries[i].size == d->resolutions.entries[0].size);
        for (j = 0; j < sizeof(struct angler_csi); j++)
            REQUIRE(((uint8_t *)&d->csi.entries[i])[j] == ((uint8_t *)&d->csi.entries[0])[j]);
        /* The daemon takes a mode for a 30 fps request only if max >= 30.0. */
        REQUIRE(m->maximum_fps >= 30.0f && m->maximum_fps < 50.0f);
        REQUIRE(d->modes.entries[0].maximum_fps < 30.0f);
    }
    REQUIRE(d->slave.power_setting_array.power_setting_a[5].config_val == PROFILE_MCLK);
#ifdef CITYMAN_CAMERA_NUBIA24
    out = d->init.entries[d->init.count - 1];
    REQUIRE(out.size == 413 && d->resolutions.entries[0].size == 173);
    out = d->resolutions.entries[0];
    REQUIRE(read_word(out.reg_setting, out.size, 0x080a) == 0x00a7);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0816) == 0x017f);
#endif
    for (i = 0; i < d->slave.power_setting_array.size; i++)
        REQUIRE(d->slave.power_setting_array.power_setting_a[i].seq_type != SENSOR_VREG ||
                d->slave.power_setting_array.power_setting_a[i].seq_val != CAM_VIO);
    return 0;
}
