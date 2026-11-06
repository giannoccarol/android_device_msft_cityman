/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SKELETON ONLY - Lumia 950 XL (Cityman) iris camera, test stream.
 *
 * Module "Jopo" (Foxconn), Toshiba SMIA sensor, monochrome, IR filter.
 * Wiring from the Lumia ACPI DSDT (CAMT / QCOM2436) and the kernel node
 * qcom,camera@2 of branch test/cityman-iris:
 *   CSIPHY2 in 2+1 combo mode (PHY lanes 3/4) -> CSID3, one data lane
 *   MCLK1 9.6 MHz, reset GPIO 102, standby/enable GPIO 106
 *   VDIG = PM8994 L3 1.2 V, VANA = L17 2.8 V, VIO = LVS1 1.8 V
 *
 * From the stock CDCC 0C086101.dcc (generate-ck71ir.py):
 *   one mode, 1920x1088 RAW10 (array 1928x1088), CSI lane mode 0 (1 lane)
 *   PLL: 9.6 MHz / 1 * 146, vt_pix_clk_div 2, vt_sys_clk_div 10
 *        -> 70.08 MHz pixel rate; requested link 700.8 Mbps
 *   line length 2100, min frame length 1116 -> 29.9 fps
 *   analogue gain code 56..448, gain = code / 56 (1x..8x)
 *   coarse integration margin 6 lines
 *
 * Verified on hardware: CCI master 0, slave 0x6c, ID 0x0861 at 0x0000.
 * NOT VERIFIED ON HARDWARE: lane assignment (0x0004, data on PHY lane 4 per msm_csiphy combo logic),
 * settle count, black level, mount angle. The register tables come from a
 * proprietary DCC and must not be committed to a public tree.
 */
#include "angler_sensor_abi.h"
#include "ck71ir_registers.generated.h"

#define SETTING(a, type, wait) { .reg_setting = (a), .size = COUNT(a), \
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR, .data_type = (type), .delay = (wait) }
#define POWER(type, value, config, wait) { .seq_type = (type), .seq_val = (value), \
    .config_val = (config), .delay = (wait) }

#define CK71IR_MCLK         9600000u
#define CK71IR_LINE_LENGTH  2100u
#define CK71IR_FRAME_LINES  1116u
#define CK71IR_PIXEL_CLOCK  70080000u
#define CK71IR_MAX_FPS      29.9f
#define CK71IR_MARGIN       6u
#define CK71IR_MAX_LINES    (32767u - CK71IR_MARGIN)
#define CK71IR_GAIN_MIN     56u
#define CK71IR_GAIN_MAX     448u

#define CK71IR_REG_COARSE   0x0202
#define CK71IR_REG_GAIN     0x0204

static struct msm_camera_i2c_reg_array reset[] = {{0x0103, 1, 0}};
static struct msm_camera_i2c_reg_array stream_on[] = {
    /*
     * The DCC mode has no frame length or exposure. Seed a long exposure
     * for a first picture without the illuminator: 1024 lines = 30.7 ms.
     * The Windows production value is 350 us (DCC block 0x000e) with the
     * IR LED on.
     */
    {0x0104, 1, 0},
    {0x0340, CK71IR_FRAME_LINES >> 8, 0}, {0x0341, CK71IR_FRAME_LINES & 0xff, 0},
    {0x0202, 0x04, 0}, {0x0203, 0x00, 0},
    {0x0204, 0x00, 0}, {0x0205, CK71IR_GAIN_MIN, 0},
    {0x0104, 0, 0},
    {0x0100, 1, 0},
};
static struct msm_camera_i2c_reg_array stream_off[] = {{0x0100, 0, 0}};
static struct msm_camera_i2c_reg_array hold_on[]  = {{0x0104, 1, 0}};
static struct msm_camera_i2c_reg_array hold_off[] = {{0x0104, 0, 0}};

static int32_t ck71ir_calculate_exposure(float gain, uint32_t lines,
                                         struct angler_exposure *out)
{
    union { float f; uint32_t bits; } value = { .f = gain };
    float analog;
    uint32_t code;
    if (!out || (value.bits & 0x7f800000u) == 0x7f800000u || gain <= 0.0f)
        return -22;
    if (gain < 1.0f) gain = 1.0f;
    analog = gain > 8.0f ? 8.0f : gain;
    code = (uint32_t)(analog * (float)CK71IR_GAIN_MIN);
    if (code < CK71IR_GAIN_MIN) code = CK71IR_GAIN_MIN;
    if (code > CK71IR_GAIN_MAX) code = CK71IR_GAIN_MAX;
    if (lines < 1u) lines = 1u;
    if (lines > CK71IR_MAX_LINES) lines = CK71IR_MAX_LINES;
    out->register_gain = code;
    out->lines = lines;
    out->sensor_digital_gain = 0x100;
    out->sensor_real_gain = (float)code / (float)CK71IR_GAIN_MIN;
    out->sensor_real_digital_gain = 1.0f;
    out->isp_digital_gain = gain / out->sensor_real_gain;
    return 0;
}

static int32_t ck71ir_fill_exposure(uint32_t gain, uint32_t digital_gain,
                                    uint32_t lines, uint32_t frame_lines,
                                    int32_t luma, uint32_t hdr,
                                    struct msm_camera_i2c_reg_setting *out)
{
    struct msm_camera_i2c_reg_array *r;
    (void)digital_gain; (void)luma;
    if (!out || !out->reg_setting || hdr) return -22;
    if (gain < CK71IR_GAIN_MIN) gain = CK71IR_GAIN_MIN;
    if (gain > CK71IR_GAIN_MAX) gain = CK71IR_GAIN_MAX;
    if (lines < 1u) lines = 1u;
    if (lines > CK71IR_MAX_LINES) lines = CK71IR_MAX_LINES;
    if (frame_lines < CK71IR_FRAME_LINES) frame_lines = CK71IR_FRAME_LINES;
    if (frame_lines < lines + CK71IR_MARGIN) frame_lines = lines + CK71IR_MARGIN;
    if (frame_lines > 32767u) frame_lines = 32767u;
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

struct angler_sensor_descriptor cityman_ck71ir_descriptor = {
    .slave = {
        /*
         * The HAL matches this name to keep the camera last and hidden
         * (persist.vendor.camera.cityman.iris). Slave 0x6c on CCI master 0
         * answers with model 0x0861 at 0x0000 (cci_probe 2, 6/11/2026).
         */
        .sensor_name = "ck71ir", .camera_id = CAMERA_2, .slave_addr = 0x6c,
        .i2c_freq_mode = I2C_FAST_MODE, .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
        .sensor_id_info = { .sensor_id_reg_addr = 0x0000, .sensor_id = 0x0861 },
        .power_setting_array = {
            .power_setting_a = {
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
                POWER(SENSOR_VREG, CAM_VIO, 0, 1),
                POWER(SENSOR_VREG, CAM_VDIG, 0, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_STANDBY, GPIO_OUT_HIGH, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, CK71IR_MCLK, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_HIGH, 5),
            }, .size = 7,
            .power_down_setting_a = {
                POWER(SENSOR_GPIO, SENSOR_GPIO_RESET, GPIO_OUT_LOW, 1),
                POWER(SENSOR_CLK, SENSOR_CAM_MCLK, 0, 1),
                POWER(SENSOR_GPIO, SENSOR_GPIO_STANDBY, GPIO_OUT_LOW, 1),
                POWER(SENSOR_VREG, CAM_VDIG, 0, 1),
                POWER(SENSOR_VREG, CAM_VIO, 0, 1),
                POWER(SENSOR_VREG, CAM_VANA, 0, 1),
            }, .size_down = 6,
        },
        .is_init_params_valid = 1,
        .sensor_init_params = {.modes_supported = CAMERA_MODE_2D_B,
                               .position = FRONT_CAMERA_B, .sensor_mount_angle = 90},
        .is_flash_supported = 0,
    },
    /*
     * Monochrome sensor: no colour filter. The daemon still needs a Bayer
     * order; with unity white balance every order gives the same grey
     * picture. The RDI dump (mm-qcamera-app) is the real IR image.
     */
    .output = {0, 1, 1, 0},
    .output_registers = {0x034c, 0x034e, 0x0342, 0x0340},
    .exposure_registers = {CK71IR_REG_COARSE, CK71IR_REG_GAIN, CK71IR_MARGIN},
    .aec = {1.0f, 8.0f, 8.0f, CK71IR_MAX_LINES},
    .exposure_start_delay = 1, .exposure_update_delay = 2,
    .lens = {.focal_length = 0.0f, .pixel_pitch = 0.0f, .f_number = 2.0f,
             .sensing_method = 2, .crop_factor = 1.0f,
             .physical_width = 0.0f, .physical_height = 0.0f},
    .active_array = {1920, 1088, 0, 0, 0, 0},
    .white_level = 1023, .black_level = {64, 64, 64, 64},
    .streams = {.entries = {{.cid_count = 1,
                    .cid = {{0, CSI_RAW10, CSI_DECODE_10BIT}}, .pixel_format = {0}}}, .count = 1},
    .start = SETTING(stream_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .stop = SETTING(stream_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_on = SETTING(hold_on, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .group_off = SETTING(hold_off, MSM_CAMERA_I2C_BYTE_DATA, 0),
    .init = {.entries = {
        SETTING(reset, MSM_CAMERA_I2C_BYTE_DATA, 20),
        SETTING(ck71ir_init, MSM_CAMERA_I2C_BYTE_DATA, 0),
    }, .count = 2},
    .resolutions = {.entries = {SETTING(ck71ir_mode0_1920x1088, MSM_CAMERA_I2C_BYTE_DATA, 0)}, .count = 1},
    /* Same action order as the front sensor module. */
    .resolution_actions = {{0, 2, 4, 5, 6, 3, 1}, 7},
    .modes = {.entries = {
        {.width = 1920, .height = 1088, .line_length = CK71IR_LINE_LENGTH,
         .frame_length = CK71IR_FRAME_LINES,
         .vt_pixel_clock = CK71IR_PIXEL_CLOCK, .output_pixel_clock = CK71IR_PIXEL_CLOCK,
         .binning_factor = 1, .minimum_fps = 7.5f, .maximum_fps = CK71IR_MAX_FPS, .kind = 1},
    }, .count = 1},
    /*
     * CSIPHY2 in combo mode: the front camera keeps PHY lanes 0/1(clk)/2,
     * the iris gets PHY lanes 3 and 4 (lane_mask 0x18). msm_csiphy treats
     * lane 3 as the second clock lane in combo mode, so the data lane is
     * PHY 4 and CSID3 lane 0 reads it (lane_assign 0x0004). If the CSID
     * reports only ECC errors try 0x0003. Settle: the front works at 8-20
     * for 499 Mbps; 700.8 Mbps needs a shorter one, sweep 6-20 from 12.
     */
    .csi = {.entries = {{
        .csid = {.lane_cnt = 1, .lane_assign = 0x0004, .phy_sel = 2,
                 .lut_params = {.num_cid = 2, .vc_cfg_a = {
                     {0, CSI_RAW10, CSI_DECODE_10BIT},
                     {1, CSI_EMBED_DATA, CSI_DECODE_8BIT}}}},
        .phy = {.lane_cnt = 1, .settle_cnt = 12, .lane_mask = 0x18,
                .combo_mode = 1, .csid_core = 3},
    }}, .count = 1},
    .crops = {.entries = {{.top = 0, .bottom = 0, .left = 0, .right = 0}}, .count = 1},
    /*
     * The daemon dereferences the chromatix list at sensor start. These
     * are the angler front (imx179) blobs, as for the first VS6955 tests:
     * placeholders, not IR tuning.
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
    .exposure = {ck71ir_calculate_exposure, ck71ir_fill_exposure, 0, 0},
    .exposure_table_capacity = 8, .initial_frame_skip = 2,
};

__attribute__((visibility("default"))) void *sensor_open_lib(void)
{
    return &cityman_ck71ir_descriptor;
}
