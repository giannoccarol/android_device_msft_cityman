/*
 * Cityman (Lumia 950 XL) rear lens actuator library.
 *
 * Taken from hraj9258's talkman port (Himanshu Raj,
 * gitlab.com/hraj9258/android_device_msft_talkman, lineage-18.1-talkman,
 * camera/sensors/imx230/actuator_lib_talkman.c). The rear module and its lens
 * controller are the same on the 950 and the 950 XL. Changes for cityman: the
 * library name and the lens positions, which come from this phone's module
 * NVM (dumps/af-diag-20261027, decoded with the same running sums):
 *   far end 626, infinity 909, mid 1178, near end 1452.
 * The original notes below refer to the talkman module's values.
 */
/*
 * libactuator_talkman.so -- MSM actuator driver library for the Lumia 950
 * (talkman) rear lens.
 *
 * The lens is a separate I2C device on CCI master 1 at 8-bit address 0x7C.
 * SMIA++ names its registers through the sensor's own capability block:
 *
 *   0x1b40 ACTUATOR_TYPE            actuator class
 *   0x1b42 AF_DEVICE_ADDRESS        I2C address of the lens driver (0x7C)
 *   0x1b44 FOCUS_CHANGE_ADDRESS     first register of the position pair
 *
 * All three read back 0 from the sensor on this module; Windows Phone gets
 * them from the CDCC `config_override` block instead, which is why the values
 * below are constants rather than something the kernel discovers.  The
 * derived registers follow the WP rear-sensor KMD
 * (qccamrearsensor_primarySMIApp8992.sys, cam_drv_read_af_vital_info):
 *
 *   trigger = FOCUS_CHANGE_ADDRESS + 2   (0x05F2)
 *   busy    = FOCUS_CHANGE_ADDRESS + 3   (0x05F3)
 *
 * and its cam_drv_lens_move writes, for every move:
 *
 *   0x05F0 = pos >> 8      (one byte; the device does not auto-increment)
 *   0x05F1 = pos & 0xFF
 *   0x05F2 = 0x02          \ the "apply" trigger, re-armed on each move
 *   0x05F3 = 0x01          /
 *
 * msm_actuator's ACTUATOR_VCM parser emits a two-register DAC low byte first
 * and the high byte second, and this lens drops that high byte: written back
 * to back, 0x05F1 always takes the new value while 0x05F0 keeps its old one.
 * Hence ACTUATOR_VCM_BE, which is the same actuator with the pair swapped.
 *
 * The trigger rows cannot carry a per-entry constant on this path -- the
 * non-WRITE_DAC branch of msm_actuator_parse_i2c_params takes its data from
 * (hw_params & hw_mask) >> hw_shift -- so hw_params is 0x0201 in every damping
 * entry and the two trigger rows mask the halves out of it.
 *
 * The per-entry addr_type/data_type are only read by the BIVCM handler, but
 * translate_actuator_reg_params runs them through
 * sensor_sdk_util_get_kernel_i2c_{addr,data}_type for every row regardless and
 * logs "Invalid addr_type = 0" for a zero, so they are filled in.
 *
 * ABI (actuator_driver_params_t) recovered from libmmcamera2_sensor_modules.so
 * (af_actuator_set_params / af_actuator_move_focus / actuator_set_position)
 * and cross-checked against the ROM's own libactuator_lc898212xd.so.  See
 * senslib/ABI.md.
 */

#define MAX_ACT_NAME        32
#define MAX_ACT_REG_TBL     8
#define MAX_ACT_INIT_SET    120
#define MAX_ACT_REGION      5
#define MAX_ACT_SCENARIO    8

/* msm_actuator_write_type */
#define WRITE_HW_DAMP       0
#define WRITE_DAC           1
#define WRITE               2
#define WRITE_DIR_REG       3
#define POLL                4

/* actuator_type */
#define ACTUATOR_VCM        0
#define ACTUATOR_PIEZO      1
#define ACTUATOR_HVCM       2
#define ACTUATOR_BIVCM      3
#define ACTUATOR_VCM_BE     4   /* two-register DAC, high half written first */

/* i2c addr / data widths -- the sensor SDK enum passes straight through to
 * the kernel one (sensor_sdk_util_get_kernel_i2c_{addr,data}_type). */
#define I2C_BYTE            1
#define I2C_WORD            2

/* i2c_operation for reg_settings_t */
#define I2C_OP_WRITE        0
#define I2C_OP_POLL         1

#define LENS_I2C_ADDR       0x7C        /* 0x1b42, 8-bit form; kernel >>1 */
#define LENS_REG_POS_HI     0x05F0      /* 0x1b44 */
#define LENS_REG_POS_LO     0x05F1
#define LENS_REG_TRIGGER    0x05F2      /* 0x1b44 + 2 */
#define LENS_REG_BUSY       0x05F3      /* 0x1b44 + 3 */
#define LENS_TRIGGER_WORD   0x0201      /* 0x05F2 = 0x02, 0x05F3 = 0x01 */

#define LENS_DAC_BITS       11          /* 0 .. 2047 */

/*
 * Step table.  msm_actuator_init_step_table fills
 *     step_position_table[i] = initial_code + i * code_per_step / qvalue
 *
 * The endpoints are the module's own AF calibration, decoded out of its NVM
 * by wp/nvm-af.py (the WP KMD stores deltas; positions are running sums):
 *
 *     654   park / rest      (the downward group's entry 0; where
 *                             cam_drv_SMIApp_lens_off finally leaves the lens)
 *     900   infinity         (lens_off's first stop on the way to park, and
 *                             so the natural default)
 *    1149   mid
 *    1437   near end / macro (the fully cumulative level+up value)
 *
 * Higher code = nearer focus, confirmed by sweeping lens_pos against a
 * Laplacian focus metric.  The search therefore runs infinity -> macro.
 *
 * The step size is a MICROMETRE CALIBRATION, not an arbitrary choice.
 * af_single_get_scan_step multiplies the AF tuning's scan-step tables by
 * `step_index_per_um` to get actuator steps -- on device:
 *
 *     af_single_get_scan_step: 1 step pre-scan 8, finescan 3 step_per_um 1
 *
 * so those tables are in micrometres, they are already lens-independent, and
 * the only lens-specific number is steps-per-micrometre.  That field is a u16,
 * so it can only be 1 for any sane step size, which means one step has to be
 * as close to one micrometre as the step table can make it.
 *
 * Full travel follows from the optics: an object at .near_focus_m (0.1 m)
 * focuses at v = f*u/(u-f) = 4.5*100/95.5 = 4.712 mm, i.e. 212 um past the
 * infinity position.  So the calibrated 900..1437 span (537 codes) is 212 um
 * and one micrometre is 2.533 codes.
 *
 * 2.533 is not expressible.  `qvalue`, the framework's fixed-point divisor for
 * exactly this, never reaches the kernel.  Proven byte by byte: this record
 * left the library as
 *
 *     0c 01 00 00 02 00 34 12 cd ab 00 00     (pad 0x1234, qvalue 0xABCD)
 *
 * and msm_actuator_set_param copied
 *
 *     0c 01 00 00 02 00 00 00 01 00 00 00
 *
 * -- step_bound and code_per_step come from here, offset 6 is never copied
 * (translate_actuator_region_params reads only offsets 0, 2, 4 and 8), and
 * offset 8 arrives as a hard 1 that is neither ours nor zero.  20, 15 and
 * 0xABCD all became 1, so the daemon substitutes it and the field is unusable
 * on this build.  code_per_step therefore has to be a whole number.
 *
 * 2 is the right whole number, and not only by elimination: 268 steps of two
 * codes is 268 um of travel, which is the infinity-to-80mm distance.  Whether
 * that or the 212 um of infinity-to-100mm is this module's real range is
 * exactly the open question -- `near_focus_m` is an assumption, not a
 * measurement -- and at 80 mm this table is exact, one step being 0.9989 um.
 * At 100 mm it is 21% short, which is the safe direction anyway: a finer scan
 * is slower but cannot step over the peak, where 3 codes per step would be 18%
 * coarse and could.
 *
 * So total_steps is the travel in micrometres after all, and the number to
 * settle is the minimum focus distance, not qvalue.
 *
 * Measure it by focusing on a target at a known distance and reading the code
 * back: code(u) = 900 + (f*u/(u-f) - f) / um_per_code.
 */
/* cityman module NVM; the talkman module read 654 / 900 / 1149 / 1437 */
#define LENS_POS_PARK       626
#define LENS_POS_INFINITY   909
#define LENS_POS_MID        1178
#define LENS_POS_MACRO      1452

#define LENS_TOTAL_STEPS    271     /* 271 x 2 codes = 909..1451 */
#define LENS_CODE_PER_STEP  2       /* 0.79 um; 2.533 would be exact */
#define LENS_QVALUE         1       /* anything else never arrives */
#define LENS_INITIAL_CODE   LENS_POS_INFINITY

struct act_reg_params {
    unsigned int   reg_write_type;
    unsigned int   hw_mask;
    unsigned short reg_addr;
    unsigned short hw_shift;
    unsigned short data_shift;
    unsigned short data_type;
    unsigned short addr_type;
    unsigned short reg_data;
    unsigned short delay;
    unsigned short pad;
};

struct act_reg_settings {
    unsigned short reg_addr;
    unsigned short pad0;
    unsigned int   addr_type;
    unsigned short reg_data;
    unsigned short pad1;
    unsigned int   data_type;
    unsigned int   i2c_operation;
    unsigned int   delay;
};

struct act_region_params {
    unsigned short step_bound[2];   /* [0] = MOVE_NEAR, [1] = MOVE_FAR */
    unsigned short code_per_step;
    unsigned short pad;
    unsigned int   qvalue;
};

struct act_damping_params {
    unsigned int damping_step;
    unsigned int damping_delay;
    unsigned int hw_params;
};

struct actuator_driver_params {
    char   vendor[MAX_ACT_NAME];                            /* +0x000 */
    char   part[MAX_ACT_NAME];                              /* +0x020 */
    unsigned int   id;                                      /* +0x040 */
    unsigned int   i2c_addr;                                /* +0x044 */
    unsigned int   i2c_data_type;                           /* +0x048 */
    unsigned int   i2c_addr_type;                           /* +0x04c */
    unsigned int   act_type;                                /* +0x050 */
    unsigned short data_size;                               /* +0x054 */
    unsigned short pad_056;
    unsigned char  reg_tbl_size;                            /* +0x058 */
    unsigned char  pad_059[3];
    struct act_reg_params   reg_tbl[MAX_ACT_REG_TBL];       /* +0x05c */
    unsigned short init_setting_size;                       /* +0x11c */
    unsigned short pad_11e;
    struct act_reg_settings init_settings[MAX_ACT_INIT_SET];/* +0x120 */
    unsigned short scenario_size[2];                        /* +0xc60 */
    unsigned short scenario_step_bound[2][MAX_ACT_SCENARIO];/* +0xc64 */
    short          initial_code;                            /* +0xc84 */
    unsigned short region_size;                             /* +0xc86 */
    struct act_region_params region_params[MAX_ACT_REGION]; /* +0xc88 */
    /* park_lens is read out of damping[0][0] */
    struct act_damping_params
        damping[MAX_ACT_SCENARIO][MAX_ACT_REGION];          /* +0xcc4 */
    struct act_damping_params
        ringing[MAX_ACT_SCENARIO][MAX_ACT_REGION];          /* +0xea4 */
};                                                          /* size 0x1084 */

/*
 * One damping entry per region, repeated for every region and scenario.
 * damping_step larger than the whole DAC range makes msm_actuator_write_focus
 * emit exactly one position per move instead of a ramp; hw_params carries the
 * trigger constant that reg_tbl[2] and reg_tbl[3] mask apart.
 */
#define DAMP { 4096, 1000, LENS_TRIGGER_WORD }
#define DAMP5 { DAMP, DAMP, DAMP, DAMP, DAMP }
#define DAMP40 { DAMP5, DAMP5, DAMP5, DAMP5, DAMP5, DAMP5, DAMP5, DAMP5 }

static struct actuator_driver_params cityman_actuator = {
    .vendor            = "microsoft",
    .part              = "cityman",
    .id                = 1,
    .i2c_addr          = LENS_I2C_ADDR,
    .i2c_data_type     = I2C_BYTE,
    .i2c_addr_type     = I2C_WORD,
    .act_type          = ACTUATOR_VCM_BE,
    .data_size         = LENS_DAC_BITS,
    .reg_tbl_size      = 4,
    .reg_tbl = {
        /*
         * [0] and [1] are one WRITE_DAC pair.  ACTUATOR_VCM_BE writes
         * (code >> 8) to reg_tbl[0].reg_addr and (code & 0xff) to
         * reg_tbl[1].reg_addr; plain ACTUATOR_VCM is the other way round,
         * which this lens silently ignores -- it takes the low byte and
         * drops any high byte written after it.
         */
        { .reg_write_type = WRITE_DAC, .reg_addr = LENS_REG_POS_HI,
          .addr_type = I2C_WORD, .data_type = I2C_BYTE },
        { .reg_write_type = WRITE_DAC, .reg_addr = LENS_REG_POS_LO,
          .addr_type = I2C_WORD, .data_type = I2C_BYTE },
        /* the apply trigger, taken out of hw_params (0x0201) */
        { .reg_write_type = WRITE, .reg_addr = LENS_REG_TRIGGER,
          .hw_mask = 0x0000ff00u, .hw_shift = 8,
          .addr_type = I2C_WORD, .data_type = I2C_BYTE },
        { .reg_write_type = WRITE, .reg_addr = LENS_REG_BUSY,
          .hw_mask = 0x000000ffu, .hw_shift = 0,
          .addr_type = I2C_WORD, .data_type = I2C_BYTE },
    },
    .init_setting_size = 0,     /* the trigger is re-armed on every move */

    .scenario_size       = { 1, 1 },
    .scenario_step_bound = { { LENS_TOTAL_STEPS }, { LENS_TOTAL_STEPS } },

    .initial_code = LENS_INITIAL_CODE,
    .region_size  = 1,
    .region_params = {
        { .step_bound   = { LENS_TOTAL_STEPS, 0 },
          .code_per_step = LENS_CODE_PER_STEP,
          .qvalue        = LENS_QVALUE },
    },

    .damping = DAMP40,
    .ringing = DAMP40,
};

void *actuator_driver_open_lib(void)
{
    return &cityman_actuator;
}
