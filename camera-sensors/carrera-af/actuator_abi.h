/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Independently specified layout of the 32-bit angler actuator driver
 * library data (the object returned by actuator_driver_open_lib()).
 *
 * The offsets were measured on the vendor reference
 * libactuator_lc898212xd.so (Nexus 5X blob shipped in vendor_msft_cityman,
 * .data at 0x4000, object at 0x4004, size 0x1084) and are asserted below.
 * Kernel-facing records come from the GPL kernel SDK header.
 * This is not the Qualcomm proprietary actuator SDK header.
 */
#ifndef CITYMAN_ANGLER_ACTUATOR_ABI_H
#define CITYMAN_ANGLER_ACTUATOR_ABI_H

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT16_TYPE__ int16_t;
#include <media/msm_camsensor_sdk.h>

#define ACT_ABI_OFFSET(t, m, v) \
    _Static_assert(__builtin_offsetof(t, m) == (v), #m " actuator ABI offset")
#define ACT_ABI_SIZE(t, v) _Static_assert(sizeof(t) == (v), #t " actuator ABI size")

struct angler_damping {
    struct damping_params_t ringing_params[MAX_ACTUATOR_REGION];
};

struct angler_actuator_params {
    char module_name[MAX_ACT_MOD_NAME_SIZE];
    char actuator_name[MAX_ACT_NAME_SIZE];
    uint32_t i2c_freq_mode;          /* 1 (fast) in the reference */
    uint32_t i2c_addr;               /* 8-bit CCI address */
    uint32_t i2c_data_type;          /* enum msm_actuator_data_type */
    uint32_t i2c_addr_type;          /* enum msm_actuator_addr_type */
    uint32_t act_type;               /* enum actuator_type */
    uint16_t data_size;              /* bits: kernel max code is 1 << data_size */
    uint16_t pad0;
    uint32_t reg_tbl_size;
    struct msm_actuator_reg_params_t reg_tbl[MAX_ACTUATOR_REG_TBL_SIZE];
    uint16_t init_setting_size;
    uint16_t pad1;
    struct reg_settings_t init_settings[MAX_ACTUATOR_INIT_SET];
};

struct angler_actuator_tuned_params {
    uint16_t scenario_size[NUM_ACTUATOR_DIR];
    uint16_t ringing_scenario[NUM_ACTUATOR_DIR][MAX_ACTUATOR_SCENARIO];
    int16_t initial_code;
    uint16_t region_size;
    struct region_params_t region_params[MAX_ACTUATOR_REGION];
    struct angler_damping damping[NUM_ACTUATOR_DIR][MAX_ACTUATOR_SCENARIO];
};

struct angler_actuator_driver {
    struct angler_actuator_params actuator_params;
    struct angler_actuator_tuned_params actuator_tuned_params;
};

ACT_ABI_SIZE(struct msm_actuator_reg_params_t, 24);
ACT_ABI_SIZE(struct reg_settings_t, 24);
ACT_ABI_SIZE(struct region_params_t, 12);
ACT_ABI_SIZE(struct damping_params_t, 12);
ACT_ABI_OFFSET(struct angler_actuator_params, actuator_name, 0x20);
ACT_ABI_OFFSET(struct angler_actuator_params, i2c_addr, 0x44);
ACT_ABI_OFFSET(struct angler_actuator_params, act_type, 0x50);
ACT_ABI_OFFSET(struct angler_actuator_params, data_size, 0x54);
ACT_ABI_OFFSET(struct angler_actuator_params, reg_tbl_size, 0x58);
ACT_ABI_OFFSET(struct angler_actuator_params, reg_tbl, 0x5c);
ACT_ABI_OFFSET(struct angler_actuator_params, init_setting_size, 0x11c);
ACT_ABI_OFFSET(struct angler_actuator_params, init_settings, 0x120);
ACT_ABI_OFFSET(struct angler_actuator_driver, actuator_tuned_params, 0xc60);
ACT_ABI_OFFSET(struct angler_actuator_driver, actuator_tuned_params.initial_code, 0xc84);
ACT_ABI_OFFSET(struct angler_actuator_driver, actuator_tuned_params.region_params, 0xc88);
ACT_ABI_OFFSET(struct angler_actuator_driver, actuator_tuned_params.damping, 0xcc4);
ACT_ABI_SIZE(struct angler_actuator_driver, 0x1084);

#endif
