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
    REQUIRE(d == &cityman_imx230_descriptor);
    REQUIRE(d->modes.count == 1 && d->resolutions.count == 1 && d->csi.count == 1);
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
    REQUIRE(read_word(buffer.r, out.size, 0x0340) == (PROFILE_FRAME_LINES > 3008 ? PROFILE_FRAME_LINES : 3008));
    REQUIRE(d->exposure.fill(0, 0x100, 0, 0, 0, 1, &out) < 0);
    for (i = 0; i < d->init.count; i++)
        for (j = 0; j < d->init.entries[i].size; j++) {
            uint16_t a = d->init.entries[i].reg_setting[j].reg_addr;
            REQUIRE(a != 0 && a != 0x0100 && a != 0x0107);
        }
    REQUIRE(d->init.entries[d->init.count - 1].size > 12 && d->resolutions.entries[0].size > 12);
    out = d->resolutions.entries[0];
    REQUIRE(read_word(out.reg_setting, out.size, 0x034c) == PROFILE_WIDTH);
    REQUIRE(read_word(out.reg_setting, out.size, 0x034e) == PROFILE_HEIGHT);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0112) == 0x0a0a);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0340) == d->modes.entries[0].frame_length);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0342) == PROFILE_LINE_PIXELS);
    REQUIRE(PROFILE_MCLK / read_word(out.reg_setting, out.size, 0x0304) *
            read_word(out.reg_setting, out.size, 0x0306) /
            (read_word(out.reg_setting, out.size, 0x0302) *
             read_word(out.reg_setting, out.size, 0x0300)) * 4 == PROFILE_VT_CLOCK);
    REQUIRE(PROFILE_MCLK / read_word(out.reg_setting, out.size, 0x030c) *
            read_word(out.reg_setting, out.size, 0x030e) /
            (read_word(out.reg_setting, out.size, 0x030a) *
             read_word(out.reg_setting, out.size, 0x0308)) * 4 == PROFILE_OUTPUT_CLOCK);
    REQUIRE(read_word(out.reg_setting, out.size, 0x0820) == PROFILE_OUTPUT_CLOCK / 100000);
    REQUIRE(d->csi.entries[0].phy.settle_cnt == PROFILE_SETTLE);
    REQUIRE(d->csi.entries[0].csid.lut_params.num_cid == PROFILE_CIDS);
    REQUIRE(d->slave.power_setting_array.power_setting_a[5].config_val == PROFILE_MCLK);
    out = d->init.entries[d->init.count - 1];
    REQUIRE(read_word(out.reg_setting, out.size, 0x0136) == (PROFILE_MCLK / 1000 * 256) / 1000);
#ifdef CITYMAN_CAMERA_NUBIA24
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
