/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SKELETON ONLY - Lumia 950 XL (Cityman) front camera, ST VS6955CA.
 *
 * Wiring from the Lumia ACPI DSDT (CAMF / QCOM2439) and the current
 * Android device tree node qcom,camera@1:
 *   CCI master 0, slave address 0x20, 16-bit register address
 *   CSIPHY2 / CSID2, MCLK2 9.6 MHz, reset GPIO 104, standby GPIO 105
 *   VANA = PM8994 L17 2.8 V, VIO = PM8994 LVS1 1.8 V, no VAF rail
 *
 * Fixed focus: no VCM/actuator rail and no lens/OIS/AF data in the DCC.
 * The generic SMIA++ driver contains AF code, but this module does not
 * use it. Do not add an actuator.
 *
 * TODO before enabling: bayer order, CSI lane assignment/settle, vt and
 * output pixel clocks, frame length, exposure/gain model validation,
 * per-unit calibration (OTP) and tuning.
 *
 * The register tables come from the proprietary CDCC 0A214000.dcc of the
 * stock Lumia FFU. They must not be committed to a public tree.
 */
#include "angler_sensor_abi.h"
#include "vs6955_registers.generated.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define SETTING(a, type, wait) { .reg_setting = (a), .size = COUNT(a), \
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR, .data_type = (type), .delay = (wait) }
/* Explicit size: the CCI queue cannot take the 805-entry init as one table. */
#define SETTING_N(a, n, type, wait) { .reg_setting = (a), .size = (n), \
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR, .data_type = (type), .delay = (wait) }
#define POWER(type, value, config, wait) { .seq_type = (type), .seq_val = (value), \
    .config_val = (config), .delay = (wait) }

static struct msm_camera_i2c_reg_array reset[]     = {{0x0103, 1, 0}};
/* Windows driver stream-on: lane mode, mode select, output enable. */
static struct msm_camera_i2c_reg_array stream_on[] = {
    /* MIPI PLL: EXTCLK 9.6 MHz * 52 / 1 = 499.2 Mbps per lane. */
    {0x030c, 0, 0}, {0x030d, 1, 0},
    {0x030e, 0, 0}, {0x030f, 52, 0},
    {0x0310, 1, 0},
    /* The DCC has no frame length or exposure: seed both before streaming. */
    {0x0104, 1, 0},
    {0x0340, 0x04, 0}, {0x0341, 0xba, 0},   /* 1210 lines @ 99.84 MHz = 30 fps */
    {0x0202, 0x03, 0}, {0x0203, 0xe8, 0},   /* 1000 lines */
    {0x0204, 0x00, 0}, {0x0205, 0x00, 0},   /* 1x gain */
    {0x0104, 0, 0},
    {0x0114, 1, 0},   /* two MIPI lanes */
    {0x0100, 1, 0},
    {0x0138, 1, 0},
};
static struct msm_camera_i2c_reg_array stream_off[]= {
    {0x0138, 0, 0},
    {0x0100, 0, 0},
};
static struct msm_camera_i2c_reg_array hold_on[]   = {{0x0104, 1, 0}};
static struct msm_camera_i2c_reg_array hold_off[]  = {{0x0104, 0, 0}};

/* Line length 2750 comes from every DCC mode table (0x0342/0x0343).
 * Frame length and pixel clocks still have to be derived/measured. */
#define VS6955_LINE_LENGTH 2750u
#define VS6955_MCLK        9600000u
#define VS6955_FRAME_LINES  1210u
#define VS6955_VT_CLOCK     99840000u
#define VS6955_OUTPUT_CLOCK 99840000u
#define VS6955_MAX_FPS      30.0f
#define VS6955_MAX_LINES    (65535u - 8u)

/* SMIA++ exposure/gain registers; validate on this die. */
#define VS6955_REG_COARSE  0x0202
#define VS6955_REG_GAIN    0x0204

static int32_t vs6955_calculate_exposure(float gain, uint32_t lines,
                                         struct angler_exposure *out)
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
    if (lines > VS6955_MAX_LINES) lines = VS6955_MAX_LINES;
    out->register_gain = code;
    out->lines = lines;
    out->sensor_digital_gain = 0x100;
    out->sensor_real_gain = 512.0f / (512.0f - (float)code);
    out->sensor_real_digital_gain = 1.0f;
    out->isp_digital_gain = gain / out->sensor_real_gain;
    return 0;
}

static int32_t vs6955_fill_exposure(uint32_t gain, uint32_t digital_gain,
                                    uint32_t lines, uint32_t frame_lines,
                                    int32_t luma, uint32_t hdr,
                                    struct msm_camera_i2c_reg_setting *out)
{
    struct msm_camera_i2c_reg_array *r;
    (void)digital_gain; (void)luma;
    if (!out || !out->reg_setting || hdr) return -22;
    if (gain > 448u) gain = 448u;
    if (lines < 4u) lines = 4u;
    if (lines > VS6955_MAX_LINES) lines = VS6955_MAX_LINES;
    if (frame_lines < VS6955_FRAME_LINES) frame_lines = VS6955_FRAME_LINES;
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

struct angler_sensor_descriptor cityman_vs6955_descriptor = {
    .slave = {
        .sensor_name = "vs6955", .camera_id = CAMERA_1, .slave_addr = 0x20,
        .i2c_freq_mode = I2C_STANDARD_MODE, .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
        .sensor_id_info = { .sensor_id_reg_addr = 0x0016, .sensor_id = 0x03BB },
        .power_setting_array = {
            .power_setting_a = {
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
                POWER(SENSOR_VREG, CAM_VIO, 0, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_STANDBY, GPIO_OUT_HIGH, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, VS6955_MCLK, 5),
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_HIGH, 30),
            }, .size = 6,
            .power_down_setting_a = {
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, 0, 1),
                POWER(SENSOR_VREG, CAM_VIO, 0, 1),
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
            }, .size_down = 4,
        },
        .is_init_params_valid = 1,
        .sensor_init_params = {.modes_supported = CAMERA_MODE_2D_B,
                               .position = FRONT_CAMERA_B, .sensor_mount_angle = 90},
        .is_flash_supported = 0,
    },
    /* GBRG matches the angler front reference; confirm on a captured frame. */
    .output = {0, 1, 1, 3},
    .output_registers = {0x034c, 0x034e, 0x0342, 0x0340},
    .exposure_registers = {VS6955_REG_COARSE, VS6955_REG_GAIN, 8},
    .aec = {1.0f, 8.0f, 8.0f, 65527u},
    .exposure_start_delay = 1, .exposure_update_delay = 2,
    .lens = {.focal_length = 0.0f, .pixel_pitch = 0.0f, .f_number = 2.4f,
             .sensing_method = 2, .crop_factor = 1.0f,
             .physical_width = 0.0f, .physical_height = 0.0f},
    /* Crop offsets 4,4 from the DCC; active array 2592x1944. */
    .active_array = {2592, 1944, 4, 4, 4, 4},
    .white_level = 1023, .black_level = {64, 64, 64, 64},
    .streams = {.entries = {{.cid_count = 1,
                    .cid = {{0, CSI_RAW10, CSI_DECODE_10BIT}}, .pixel_format = {0}}}, .count = 1},
    .start = SETTING(stream_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .stop = SETTING(stream_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_on = SETTING(hold_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_off = SETTING(hold_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .init = {.entries = {
        SETTING(reset, MSM_CAMERA_I2C_BYTE_DATA, 20),
        SETTING(vs6955_init, MSM_CAMERA_I2C_BYTE_DATA, 0),
    }, .count = 2},
    .resolutions = {.entries = {SETTING(vs6955_mode2_1296x972, MSM_CAMERA_I2C_BYTE_DATA, 0)}, .count = 1},
    /* Same action order as the angler front sensor. */
    .resolution_actions = {{0, 2, 4, 5, 6, 3, 1}, 7},
    .modes = {.entries = {
        /* Binned mode only: it matches the single generated resolution table. */
        {.width = 1296, .height = 972, .line_length = VS6955_LINE_LENGTH, .frame_length = VS6955_FRAME_LINES,
         .vt_pixel_clock = VS6955_VT_CLOCK, .output_pixel_clock = VS6955_OUTPUT_CLOCK, .binning_factor = 2,
         .minimum_fps = 15.0f, .maximum_fps = VS6955_MAX_FPS, .kind = 1},
    }, .count = 1},
    /* CSI configuration copied from the angler front (2 lanes, CSIPHY2/CSID2). */
    .csi = {.entries = {{
        .csid = {.lane_cnt = 2, .lane_assign = 0x0423, .phy_sel = 2,
                 .lut_params = {.num_cid = 2, .vc_cfg_a = {
                     {0, CSI_RAW10, CSI_DECODE_10BIT},
                     {1, CSI_EMBED_DATA, CSI_DECODE_8BIT}}}},
        .phy = {.lane_cnt = 2, .settle_cnt = 19, .lane_mask = 0x7, .csid_core = 2},
    }}, .count = 1},
    .crops = {.count = 1},
    /*
     * The daemon loads these during sensor start.  An empty list leaves the
     * chromatix pointer null and the daemon faults at zero+0xc084 before it
     * can enumerate either camera.  These are the angler front (imx179)
     * binned-mode seeds, ABI-compatible with this daemon; they are not
     * VS6955 calibration.
     */
    .tuning = {.entries = {{.names = {
        "libchromatix_imx179_liteon_common.so",
        "libchromatix_imx179_liteon_preview.so",
        "libchromatix_imx179_liteon_preview.so",
        "libchromatix_imx179_liteon_video_binning.so",
        "libchromatix_imx179_liteon_liveshot.so",
        "libchromatix_imx179_liteon_cpp_preview.so",
        "libchromatix_imx179_liteon_cpp_preview.so",
        "libchromatix_imx179_liteon_cpp_preview.so",
        "libchromatix_imx179_liteon_cpp_video.so",
        0,
        "libchromatix_imx179_liteon_cpp_liveshot.so",
        "libchromatix_imx179_liteon_postproc.so",
    }}}, .count = 1},
    .exposure = {vs6955_calculate_exposure, vs6955_fill_exposure, 0, 0},
    .exposure_table_capacity = 8, .initial_frame_skip = 2,
};

__attribute__((visibility("default"))) void *sensor_open_lib(void)
{
    return &cityman_vs6955_descriptor;
}
