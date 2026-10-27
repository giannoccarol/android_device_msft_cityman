/* SPDX-License-Identifier: GPL-2.0-only */
#include "angler_sensor_abi.h"
#include "profile.h"
#ifdef CITYMAN_CAMERA_NUBIA24
#include "nubia24_registers.generated.h"
#elif defined(CITYMAN_CAMERA_LUMIA20)
#include "smia_registers.h"
#include "lumia20_registers.generated.h"
#else
#include "smia_registers.h"
#endif

#define FRAME_LINES PROFILE_FRAME_LINES
#define LINE_PIXELS PROFILE_LINE_PIXELS
#define MAX_LINES (65535u - 8u)
#define SETTING(a, type, wait) { .reg_setting = (a), .size = COUNT(a), \
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR, .data_type = (type), .delay = (wait) }
#define POWER(type, value, config, wait) { .seq_type = (type), .seq_val = (value), \
    .config_val = (config), .delay = (wait) }

static struct msm_camera_i2c_reg_array reset[] = {{0x0103, 1, 0}};
#ifndef CITYMAN_CAMERA_NUBIA24
/*
 * smia_idle (the Lumia common init) makes the sensor reorder its MIPI lanes
 * (0x50b8..0x50bb = 2, 1, 3, 0): on Windows that is the lane routing. On
 * Android the CSID already reorders them (lane_assign 0x0423), so the two
 * permutations stacked and every packet header failed ECC (CSID ecc
 * 0xffff0000, no SOF; the smia9 errors at 9.6 MHz had the same cause).
 * Put the sensor lanes back in natural order; nubia24 never sets them.
 */
static struct msm_camera_i2c_reg_array lane_order[] = {
    {0x50b8, 0x00, 0}, {0x50b9, 0x01, 0}, {0x50ba, 0x02, 0}, {0x50bb, 0x03, 0},
};
static struct msm_camera_i2c_reg_array prologue[] = {
    {0x0130, 0x0280, 0}, {0x0132, 0x0133, 0}, {0x0134, 0x01cd, 0},
    {0x0136, 0x0999, 0},
};
#endif
#ifdef CITYMAN_CAMERA_LUMIA20
/*
 * The Lumia DCC mode tables set PLL, geometry and link rate but no frame
 * length, exposure or gains: the Windows driver writes them at run time.
 * smia9 and nubia24 set them in their own tables. Seed the public smia9
 * values with the lumia20 frame length (3952 lines) before streaming.
 */
static struct msm_camera_i2c_reg_array lumia20_frame[] = {
    {0x0340, PROFILE_FRAME_LINES >> 8, 0}, {0x0341, PROFILE_FRAME_LINES & 0xff, 0},
    {0x0202, 0x04, 0}, {0x0203, 0xb0, 0},
    {0x0204, 0x00, 0}, {0x0205, 0x00, 0},
    {0x020e, 0x01, 0}, {0x020f, 0x00, 0}, {0x0210, 0x01, 0}, {0x0211, 0x00, 0},
    {0x0212, 0x01, 0}, {0x0213, 0x00, 0}, {0x0214, 0x01, 0}, {0x0215, 0x00, 0},
};
#endif
static struct msm_camera_i2c_reg_array stream_on[] = {{0x0100, 1, 0}};
static struct msm_camera_i2c_reg_array stream_off[] = {{0x0100, 0, 0}};
static struct msm_camera_i2c_reg_array hold_on[] = {{0x0104, 1, 0}};
static struct msm_camera_i2c_reg_array hold_off[] = {{0x0104, 0, 0}};

static int32_t calculate_exposure(float gain, uint32_t lines, struct angler_exposure *out)
{
    union { float f; uint32_t bits; } value = { .f = gain };
    float analog;
    uint32_t code;
    if (!out || (value.bits & 0x7f800000u) == 0x7f800000u || gain <= 0.0f)
        return -22;
    if (gain < 1.0f) gain = 1.0f;
    analog = gain > 8.0f ? 8.0f : gain;
    code = (uint32_t)(512.0f - 512.0f / analog);
    if (code > 448u) code = 448u;
    if (lines < 4u) lines = 4u;
    if (lines > MAX_LINES) lines = MAX_LINES;
    out->register_gain = code;
    out->lines = lines;
    out->sensor_digital_gain = 0x100;
    out->sensor_real_gain = 512.0f / (512.0f - (float)code);
    out->sensor_real_digital_gain = 1.0f;
    out->isp_digital_gain = gain / out->sensor_real_gain;
    return 0;
}

static int32_t fill_exposure(uint32_t gain, uint32_t digital_gain, uint32_t lines,
                             uint32_t frame_lines, int32_t luma, uint32_t hdr,
                             struct msm_camera_i2c_reg_setting *out)
{
    struct msm_camera_i2c_reg_array *r;
    (void)digital_gain; (void)luma;
    if (!out || !out->reg_setting || hdr) return -22;
    if (gain > 448u) gain = 448u;
    if (lines < 4u) lines = 4u;
    if (lines > MAX_LINES) lines = MAX_LINES;
    if (frame_lines < FRAME_LINES) frame_lines = FRAME_LINES;
    if (frame_lines < lines + 8u) frame_lines = lines + 8u;
    if (frame_lines > 65535u) frame_lines = 65535u;
    r = out->reg_setting;
    r[0] = (struct msm_camera_i2c_reg_array){0x0104, 1, 0};
    r[1] = (struct msm_camera_i2c_reg_array){0x0340, frame_lines >> 8, 0};
    r[2] = (struct msm_camera_i2c_reg_array){0x0341, frame_lines & 255u, 0};
    r[3] = (struct msm_camera_i2c_reg_array){0x0202, lines >> 8, 0};
    r[4] = (struct msm_camera_i2c_reg_array){0x0203, lines & 255u, 0};
    r[5] = (struct msm_camera_i2c_reg_array){0x0204, gain >> 8, 0};
    r[6] = (struct msm_camera_i2c_reg_array){0x0205, gain & 255u, 0};
    r[7] = (struct msm_camera_i2c_reg_array){0x0104, 0, 0};
    out->size = 8;
    out->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
    out->data_type = MSM_CAMERA_I2C_BYTE_DATA;
    out->delay = 0;
    return 0;
}

__attribute__((visibility("default")))
struct angler_sensor_descriptor cityman_imx230_descriptor = {
    .slave = {
        .sensor_name = "imx230", .camera_id = CAMERA_0, .slave_addr = 0x20,
        .i2c_freq_mode = I2C_STANDARD_MODE, .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
        .sensor_id_info = { .sensor_id_reg_addr = 0x0016, .sensor_id = 0x0230 },
        .power_setting_array = {
            /* LVS1 is shared with touch: the module never votes it off. */
            .power_setting_a = {
                POWER(SENSOR_VREG, CAM_VAF, 0, 1),
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
                POWER(SENSOR_VREG, CAM_VDIG, 0, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_STANDBY, GPIO_OUT_LOW, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, PROFILE_MCLK, 25),
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_HIGH, 30),
            }, .size = 7,
            .power_down_setting_a = {
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, 0, 1),
                POWER(SENSOR_VREG, CAM_VDIG, 0, 1),
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
                POWER(SENSOR_VREG, CAM_VAF, 0, 1),
            }, .size_down = 5,
        },
        .is_init_params_valid = 1,
        .sensor_init_params = {.modes_supported = CAMERA_MODE_2D_B, .position = BACK_CAMERA_B,
                               .sensor_mount_angle = 270},
        .is_flash_supported = 0,
    },
    /* RAW Bayer, CSI, 10-bit. GBRG was selected by the on-device Bayer test. */
    .output = {0, 1, 1, 3},
    .output_registers = {0x034c, 0x034e, 0x0342, 0x0340},
    .exposure_registers = {0x0202, 0x0204, 8},
    .aec = {1.0f, 8.0f, 8.0f, MAX_LINES},
    .exposure_start_delay = 1, .exposure_update_delay = 2,
    /* Optical metadata is provisional until verified against the Cityman DCC. */
    .lens = {.focal_length = 4.26f, .pixel_pitch = 1.12f, .f_number = 1.9f,
             .sensing_method = 1, .crop_factor = 1.0f,
             .physical_width = 5.98528f, .physical_height = 4.49792f},
    .active_array = {5344, 4016, 0, 0, 0, 0},
    .white_level = 1023, .black_level = {64, 64, 64, 64},
    .streams = {.entries = {{.cid_count = 1, .cid = {{0, CSI_RAW10, CSI_DECODE_10BIT}},
                            .pixel_format = {0}}}, .count = 1},
    .start = SETTING(stream_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .stop = SETTING(stream_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_on = SETTING(hold_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_off = SETTING(hold_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .init = {.entries = {
        SETTING(reset, MSM_CAMERA_I2C_BYTE_DATA, 20),
#ifdef CITYMAN_CAMERA_NUBIA24
        SETTING(nubia24_init, MSM_CAMERA_I2C_BYTE_DATA, 0),
    }, .count = 2},
#else
        SETTING(prologue, MSM_CAMERA_I2C_WORD_DATA, 0),
        SETTING(smia_idle, MSM_CAMERA_I2C_BYTE_DATA, 0),
        SETTING(lane_order, MSM_CAMERA_I2C_BYTE_DATA, 0),
#ifdef CITYMAN_CAMERA_LUMIA20
        SETTING(lumia20_frame, MSM_CAMERA_I2C_BYTE_DATA, 0),
    }, .count = 5},
#else
    }, .count = 4},
#endif
#endif
    .resolutions = {.entries = {SETTING(PROFILE_MODE, MSM_CAMERA_I2C_BYTE_DATA, 0)}, .count = 1},
    .resolution_actions = {{0, 2, 6, 3, 4, 5, 1}, 7},
    .modes = {.entries = {{.width = PROFILE_WIDTH, .height = PROFILE_HEIGHT, .line_length = LINE_PIXELS,
        .frame_length = FRAME_LINES, .vt_pixel_clock = PROFILE_VT_CLOCK, .output_pixel_clock = PROFILE_OUTPUT_CLOCK,
        .binning_factor = PROFILE_BINNING, .minimum_fps = PROFILE_MIN_FPS, .maximum_fps = PROFILE_MAX_FPS, .kind = 1}}, .count = 1},
    .csi = {.entries = {{
        .csid = {.lane_cnt = 4, .lane_assign = 0x0423, .phy_sel = 0,
                 .lut_params = {.num_cid = PROFILE_CIDS, .vc_cfg_a = {{0, CSI_RAW10, CSI_DECODE_10BIT},
#ifdef CITYMAN_CAMERA_NUBIA24
                     {1, 0x36, CSI_DECODE_10BIT}, {3, CSI_EMBED_DATA, CSI_DECODE_8BIT},
#endif
                 }}},
        .phy = {.lane_cnt = 4, .settle_cnt = PROFILE_SETTLE, .lane_mask = 0x1f, .csid_core = 0},
    }}, .count = 1},
    .crops = {.count = 1},
    /* ABI-compatible ISP seed only; these are not IMX230/Lumia calibrations. */
    .tuning = {.entries = {{.names = {
        "libchromatix_imx377_common.so", "libchromatix_imx377_snapshot.so",
        "libchromatix_imx377_snapshot.so", "libchromatix_imx377_default_video.so",
        "libchromatix_imx377_liveshot.so", "libchromatix_imx377_cpp_preview.so",
        "libchromatix_imx377_cpp_snapshot.so", "libchromatix_imx377_cpp_snapshot.so",
        "libchromatix_imx377_cpp_video.so", "libchromatix_imx377_cpp_uhd_video.so",
        "libchromatix_imx377_cpp_liveshot.so", "libchromatix_imx377_postproc.so",
    }}}, .count = 1},
    .exposure = {calculate_exposure, fill_exposure, 0, 0},
    .exposure_table_capacity = 8, .initial_frame_skip = 2,
};

__attribute__((visibility("default"))) void *sensor_open_lib(void)
{
    return &cityman_imx230_descriptor;
}
