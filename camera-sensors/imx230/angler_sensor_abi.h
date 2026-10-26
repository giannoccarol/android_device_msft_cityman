/* SPDX-License-Identifier: GPL-2.0-only */
/* Independently specified 32-bit angler sensor plug-in interface.
 * See ABI.md for the reference binaries, consumer access sites and limits.
 * Kernel-facing records are included from the matching GPL kernel SDK.
 * This is not the Qualcomm proprietary sensor SDK header.
 */
#ifndef CITYMAN_ANGLER_SENSOR_ABI_H
#define CITYMAN_ANGLER_SENSOR_ABI_H

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT32_TYPE__ int32_t;
#include <media/msm_camsensor_sdk.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define ABI_OFFSET(t, m, v) _Static_assert(__builtin_offsetof(t, m) == (v), #m " ABI offset")
#define ABI_SIZE(t, v) _Static_assert(sizeof(t) == (v), #t " ABI size")

struct angler_output {
    uint32_t format, connection, raw_depth, bayer_order;
};
struct angler_output_registers { uint16_t x, y, line_length, frame_length; };
struct angler_exposure_registers { uint16_t coarse, gain; uint32_t margin; };
struct angler_aec_limits { float min_gain, max_gain, max_analog_gain; uint32_t max_lines; };
struct angler_lens {
    float focal_length, pixel_pitch, f_number, total_focus_distance, horizontal_angle, vertical_angle;
    uint32_t sensing_method;
    float crop_factor, minimum_focus_distance, physical_width, physical_height;
};
struct angler_pixel_array { uint32_t width, height; uint16_t left, right, top, bottom; };
struct angler_stream {
    uint16_t cid_count;
    struct msm_camera_csid_vc_cfg cid[5];
    uint32_t pixel_format[5];
};
struct angler_streams { struct angler_stream entries[5]; uint32_t count; };
struct angler_register_sets { struct msm_camera_i2c_reg_setting entries[10]; uint32_t count; };
struct angler_resolution_actions { uint32_t actions[7], count; };
struct angler_mode {
    uint16_t width, height, line_length, frame_length;
    uint32_t vt_pixel_clock, output_pixel_clock;
    uint16_t binning_factor, binning_method;
    float minimum_fps, maximum_fps;
    uint32_t kind, offset_x, offset_y, scale_factor, unused, horizontal_extra, vertical_extra;
};
struct angler_modes { struct angler_mode entries[10]; uint32_t count; };
struct angler_csi { struct msm_camera_csid_params csid; struct msm_camera_csiphy_params phy; };
struct angler_csi_modes { struct angler_csi entries[10]; uint32_t count; };
struct angler_crop { uint16_t top, bottom, left, right; };
struct angler_crops { struct angler_crop entries[10]; uint32_t count; };
struct angler_tuning { const char *names[12]; };
struct angler_tunings { struct angler_tuning entries[10]; uint32_t count; };
struct angler_exposure {
    uint32_t register_gain, lines, sensor_digital_gain;
    float sensor_real_gain, sensor_real_digital_gain, isp_digital_gain;
};
typedef int32_t (*angler_calculate_fn)(float, uint32_t, struct angler_exposure *);
typedef int32_t (*angler_fill_fn)(uint32_t, uint32_t, uint32_t, uint32_t,
                                 int32_t, uint32_t, struct msm_camera_i2c_reg_setting *);
struct angler_exposure_functions {
    angler_calculate_fn calculate;
    angler_fill_fn fill;
    void *reset_black_level;
    void *unused;
};

struct angler_sensor_descriptor {
    struct msm_camera_sensor_slave_info slave;
    struct angler_output output;
    struct angler_output_registers output_registers;
    struct angler_exposure_registers exposure_registers;
    struct angler_aec_limits aec;
    uint32_t exposure_start_delay, exposure_update_delay;
    struct angler_lens lens;
    struct angler_pixel_array active_array;
    uint32_t disabled_metadata_output[4];
    uint16_t white_level, black_level[4], level_padding;
    struct angler_streams streams;
    struct msm_camera_i2c_reg_setting start, stop, group_on, group_off;
    /* All test-pattern slots and their counts remain zero / unadvertised. */
    uint8_t disabled_test_patterns[0x300];
    struct angler_register_sets init, resolutions;
    struct angler_resolution_actions resolution_actions;
    struct angler_modes modes;
    struct angler_csi_modes csi;
    struct angler_crops crops;
    struct angler_tunings tuning;
    struct angler_exposure_functions exposure;
    uint32_t exposure_table_capacity, initial_frame_skip, hdr_frame_skip;
    /* No PDAF, HDR, external statistics parser, EEPROM or custom extensions. */
    uint8_t disabled_extensions[0x14c];
};

ABI_SIZE(void *, 4);
ABI_SIZE(struct msm_camera_sensor_slave_info, 0x61c);
ABI_SIZE(struct msm_camera_i2c_reg_array, 8);
ABI_SIZE(struct msm_camera_i2c_reg_setting, 20);
ABI_SIZE(struct angler_stream, 40);
ABI_SIZE(struct angler_mode, 56);
ABI_SIZE(struct angler_csi, 140);
ABI_SIZE(struct angler_exposure, 24);
ABI_OFFSET(struct angler_sensor_descriptor, output, 0x61c);
ABI_OFFSET(struct angler_sensor_descriptor, streams, 0x6ac);
ABI_OFFSET(struct angler_sensor_descriptor, start, 0x778);
ABI_OFFSET(struct angler_sensor_descriptor, init, 0xac8);
ABI_OFFSET(struct angler_sensor_descriptor, resolutions, 0xb94);
ABI_OFFSET(struct angler_sensor_descriptor, resolution_actions, 0xc60);
ABI_OFFSET(struct angler_sensor_descriptor, modes, 0xc80);
ABI_OFFSET(struct angler_sensor_descriptor, csi, 0xeb4);
ABI_OFFSET(struct angler_sensor_descriptor, crops, 0x1430);
ABI_OFFSET(struct angler_sensor_descriptor, tuning, 0x1484);
ABI_OFFSET(struct angler_sensor_descriptor, exposure, 0x1668);
ABI_OFFSET(struct angler_sensor_descriptor, disabled_extensions, 0x1684);
ABI_SIZE(struct angler_sensor_descriptor, 0x17d0);

extern struct angler_sensor_descriptor cityman_imx230_descriptor;
void *sensor_open_lib(void);
#endif
