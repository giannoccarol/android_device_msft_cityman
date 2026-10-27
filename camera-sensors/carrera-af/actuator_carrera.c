// SPDX-License-Identifier: GPL-2.0-only
/*
 * AF actuator driver data for the Lumia 950/950 XL rear "Carrera" module.
 *
 * Hardware facts, from the Windows SMIA++ rear driver
 * (qccamrearsensor_primarySMIApp8994.sys, ARM32) and the module DCC
 * 0AEACA05.dcc; see dumps/af-ois-plan-20261027/af-ois-plan.md:
 *   - one AF/OIS controller on the rear CCI master, 8-bit address 0x7c;
 *   - 16-bit register addresses (the driver hardcodes address size 2 for
 *     it, and polls 0x0524 at 0x7c with 16-bit addressing);
 *   - lens move = 0x05f0 <- pos[15:8], 0x05f1 <- pos[7:0],
 *     then 0x05f2 <- 0x02, 0x05f3 <- 0x01 (cam_drv_lens_move);
 *     0x05f0 is the DCC override value of capability register 0x1b44;
 *   - controller ID read from register 0x0000 before configuration.
 *
 * NOT known: the code range. The Windows driver takes infinity/macro
 * positions from the module NVM (cam_drv_SMIApp_parse_nvm_af_data).
 * Build with the values read from the NVM dump:
 *   -DCARRERA_AF_INF_CODE=<n> -DCARRERA_AF_MACRO_CODE=<n>
 * There is deliberately no default.
 */
#include "actuator_abi.h"

#if !defined(CARRERA_AF_INF_CODE) || !defined(CARRERA_AF_MACRO_CODE)
#error "Define CARRERA_AF_INF_CODE and CARRERA_AF_MACRO_CODE from the module NVM dump"
#endif
#if CARRERA_AF_MACRO_CODE <= CARRERA_AF_INF_CODE
#error "Macro code must be above infinity code; reversed polarity needs a different table"
#endif
#if CARRERA_AF_MACRO_CODE > 32767
#error "msm_actuator step table uses int16 codes"
#endif

#ifndef CARRERA_AF_OIS_INIT
#define CARRERA_AF_OIS_INIT 1
#endif

/*
 * Keep the 325-step scale of the vendor lc898212xd AF tuning (ringing
 * scenario and region boundary 0x145 in libactuator_lc898212xd.so), so the
 * reused AF algorithm tuning keeps consistent step indexes.
 */
#define CARRERA_AF_TOTAL_STEPS 325
#define CARRERA_AF_QVALUE 128
#define CARRERA_AF_CODE_PER_STEP \
    (((CARRERA_AF_MACRO_CODE - CARRERA_AF_INF_CODE) * CARRERA_AF_QVALUE + \
      CARRERA_AF_TOTAL_STEPS / 2) / CARRERA_AF_TOTAL_STEPS)
_Static_assert(CARRERA_AF_CODE_PER_STEP > 0 && CARRERA_AF_CODE_PER_STEP < 65536,
               "code_per_step out of range");

/* hw_params carries the two command bytes written after each position. */
#define CARRERA_AF_MOVE_CMD 0x0201u
/* Provisional settle time between damping writes (us); tune on the phone. */
#define CARRERA_AF_DAMPING_DELAY_US 2000u

/*
 * One write per move: a damping step larger than the whole range makes
 * msm_actuator_write_focus skip its loop and write the target directly.
 * Its loop variable is int16, so curr + step must not overflow.
 */
#define CARRERA_AF_DAMPING_STEP (CARRERA_AF_MACRO_CODE - CARRERA_AF_INF_CODE + 1)
_Static_assert(CARRERA_AF_MACRO_CODE + CARRERA_AF_DAMPING_STEP <= 32767,
               "damping step would overflow the int16 loop in msm_actuator");

#define DAMP {.ringing_params = {{CARRERA_AF_DAMPING_STEP, CARRERA_AF_DAMPING_DELAY_US, \
                                  CARRERA_AF_MOVE_CMD}}}

__attribute__((visibility("default")))
struct angler_actuator_driver cityman_carrera_actuator = {
    .actuator_params = {
        .module_name = "microsoft",
        .actuator_name = CARRERA_AF_NAME,
        .i2c_freq_mode = I2C_FAST_MODE,
        .i2c_addr = 0x7c,
        .i2c_data_type = MSM_ACTUATOR_BYTE_DATA,
        .i2c_addr_type = MSM_ACTUATOR_WORD_ADDR,
        .act_type = ACTUATOR_VCM,
        .data_size = 16,
        /*
         * msm_actuator_parse_i2c_params (VCM): a WRITE_DAC entry that is not
         * last writes the low byte to its own register and the high byte to
         * the next entry's register. Other entries write
         * (hw_params & hw_mask) >> hw_shift. Bus order is therefore
         * 0x05f1, 0x05f0, 0x05f2, 0x05f3; Windows writes 0x05f0 first.
         */
        .reg_tbl_size = 4,
        .reg_tbl = {
            {.reg_write_type = MSM_ACTUATOR_WRITE_DAC, .reg_addr = 0x05f1},
            {.reg_write_type = MSM_ACTUATOR_WRITE_DAC, .reg_addr = 0x05f0},
            {.reg_write_type = MSM_ACTUATOR_WRITE_HW_DAMP, .hw_mask = 0xff00,
             .reg_addr = 0x05f2, .hw_shift = 8},
            {.reg_write_type = MSM_ACTUATOR_WRITE_HW_DAMP, .hw_mask = 0x00ff,
             .reg_addr = 0x05f3, .hw_shift = 0},
        },
#if CARRERA_AF_OIS_INIT
        /*
         * Windows DCC block 0x18 (OIS init, applied before AF is used):
         * 0x0520 <- 0x01, poll 0x0524 != 0, 0x05f2 <- 0x02. msm_actuator can
         * only compare once, so the poll is replaced by a delay; delays need
         * the init-settings fix in msm_actuator_init_focus.
         */
        .init_setting_size = 2,
        .init_settings = {
            {0x0520, MSM_ACTUATOR_WORD_ADDR, 0x01, MSM_ACTUATOR_BYTE_DATA, MSM_ACT_WRITE, 100},
            {0x05f2, MSM_ACTUATOR_WORD_ADDR, 0x02, MSM_ACTUATOR_BYTE_DATA, MSM_ACT_WRITE, 0},
        },
#endif
    },
    .actuator_tuned_params = {
        .scenario_size = {1, 1},
        .ringing_scenario = {{CARRERA_AF_TOTAL_STEPS}, {CARRERA_AF_TOTAL_STEPS}},
        .initial_code = CARRERA_AF_INF_CODE,
        .region_size = 1,
        .region_params = {{
            .step_bound = {CARRERA_AF_TOTAL_STEPS, 0},
            .code_per_step = CARRERA_AF_CODE_PER_STEP,
            .qvalue = CARRERA_AF_QVALUE,
        }},
        .damping = {{DAMP}, {DAMP}},
    },
};

__attribute__((visibility("default")))
void *actuator_driver_open_lib(void)
{
    return &cityman_carrera_actuator;
}
