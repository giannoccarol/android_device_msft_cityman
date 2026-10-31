/*
 * libflash_hill.so -- the userspace flash driver library for the Lumia 950's
 * "Hill" three-channel LED flash IC, written from scratch.
 *
 * WHY THIS EXISTS
 *
 * The camera daemon will only talk to a flash whose v4l2 subdev publishes
 * entity.group_id == MSM_CAMERA_SUBDEV_FLASH (16).  That is msm_flash.c, the
 * newer of the two flash frameworks in this kernel; the older one
 * (msm_led_flash.c + msm_led_i2c_trigger.c, which sensor/flash/hill.c is
 * built on) publishes 11, and `module_sensor_find_other_subdev` in
 * libmmcamera2_sensor_modules.so filters entities to
 * {0,1, 7,8, 12, 15,16} before it will even open them.  So the old driver is
 * invisible to the daemon no matter how well it works, and
 * `supported flash modes cnt is 0` / `Has a flash unit: false` follow.
 *
 * msm_flash.c with `qcom,cci-master` in DT selects FLASH_DRIVER_I2C, whose
 * whole implementation is "write the register arrays userspace handed me".
 * The arrays, the slave address and the power-up sequence all arrive at
 * CFG_FLASH_INIT out of *this* library, which the daemon dlopens as
 * libflash_<flash_name>.so from the sensor library's flash_name -- exactly
 * as it dlopens libactuator_<actuator_name>.so.  Hence no Hill code in the
 * kernel on this path, and all of it here.
 *
 * WHERE THE LAYOUT COMES FROM
 *
 * `flash_init` (libmmcamera2_sensor_modules.so, ELF 0x9d10) and
 * `flash_process` (0xa78c) read the struct this file's
 * flash_driver_open_lib() returns.  Every offset below is one of their loads:
 *
 *   flash_init:    lib+0x024 -> msm_flash_init_info_t.flash_driver_type
 *                  lib+0x578 -> msm_flash_init_info_t.slave_addr
 *                  translate_camera_power_setting(dst 0x550, lib+0x028)
 *                  translate_sensor_reg_setting_array(dst 0x70, lib+0x584)
 *                  lib+(0x1d2+i)*4 = lib+0x748+4i -> flash_current[i]
 *                  lib+(0x1d8+i)*4 = lib+0x760+4i -> flash_duration[i]
 *   flash_process: lib+0x5f4 with cfg_type 2 (CFG_FLASH_OFF)
 *                  lib+0x664 with cfg_type 3 (CFG_FLASH_LOW  -- torch)
 *                  lib+0x6d4 with cfg_type 4 (CFG_FLASH_HIGH -- flash)
 *
 * The translate_* destination sizes are exactly sizeof() of the kernel's own
 * structs (0x550 and 0x70 with MAX_POWER_CONFIG 12 / MAX_I2C_REG_SET 12), and
 * the arrays sit exactly 0x70 apart here, so the library's layout is the
 * kernel's layout -- not a compat variant.  Confirmed against the one
 * reference implementation on this ROM, /vendor/lib/libflash_pmic.so, whose
 * .data is 0x770 bytes with six non-zero ones: a self-pointer at +0x000,
 * "pmic" at +0x004, and flash_driver_type = 0 (FLASH_DRIVER_PMIC) at +0x024.
 *
 * THE REGISTERS
 *
 * Hill's map was recovered from this phone's own Windows flash KMD
 * (qccamflash8992.sys, Nokia's WhiteLEDFlashDriver, HW id 0x16 at 8-bit
 * 0x60 on CCI master 0) and every value below is already verified on the
 * hardware through sensor/flash/hill.c -- preview mean 143 -> 234 -> 253 and
 * registers reading back 0xe0/0x19/0x06 lit, 0x03 dark.  See CLAUDE.md
 * "The flash and torch work" and live-csi-plan.md S42, S50.
 *
 * Freestanding, like the other libraries here: no libc, one exported symbol.
 */

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;

/* ---- the kernel's own types, from include/media/msm_camsensor_sdk.h ---- */

#define MAX_POWER_CONFIG      12
#define MAX_I2C_REG_SET       12
#define MAX_LED_TRIGGERS      3

#define GPIO_OUT_LOW          (0 << 1)
#define GPIO_OUT_HIGH         (1 << 1)

enum msm_flash_driver_type {
	FLASH_DRIVER_PMIC,
	FLASH_DRIVER_I2C,
	FLASH_DRIVER_GPIO,
	FLASH_DRIVER_DEFAULT
};

enum msm_sensor_power_seq_type_t {
	SENSOR_CLK, SENSOR_GPIO, SENSOR_VREG, SENSOR_I2C_MUX, SENSOR_I2C
};

enum msm_sensor_power_seq_gpio_t {
	SENSOR_GPIO_RESET, SENSOR_GPIO_STANDBY, SENSOR_GPIO_AF_PWDM,
	SENSOR_GPIO_VIO, SENSOR_GPIO_VANA, SENSOR_GPIO_VDIG, SENSOR_GPIO_VAF,
	SENSOR_GPIO_FL_EN, SENSOR_GPIO_FL_NOW, SENSOR_GPIO_FL_RESET,
	SENSOR_GPIO_CUSTOM1, SENSOR_GPIO_CUSTOM2, SENSOR_GPIO_MAX
};

enum msm_camera_i2c_reg_addr_type {
	MSM_CAMERA_I2C_BYTE_ADDR = 1, MSM_CAMERA_I2C_WORD_ADDR
};

enum msm_camera_i2c_data_type {
	MSM_CAMERA_I2C_BYTE_DATA = 1, MSM_CAMERA_I2C_WORD_DATA
};

struct msm_sensor_power_setting {
	enum msm_sensor_power_seq_type_t seq_type;
	uint16_t                         seq_val;
	long                             config_val;
	uint16_t                         delay;
	void                            *data[10];
};

struct msm_sensor_power_setting_array {
	struct msm_sensor_power_setting  power_setting_a[MAX_POWER_CONFIG];
	struct msm_sensor_power_setting *power_setting;
	uint16_t                         size;
	struct msm_sensor_power_setting  power_down_setting_a[MAX_POWER_CONFIG];
	struct msm_sensor_power_setting *power_down_setting;
	uint16_t                         size_down;
};

struct msm_camera_i2c_reg_array {
	uint16_t reg_addr;
	uint16_t reg_data;
	uint32_t delay;
};

struct msm_camera_i2c_reg_setting_array {
	struct msm_camera_i2c_reg_array   reg_setting_a[MAX_I2C_REG_SET];
	uint16_t                          size;
	enum msm_camera_i2c_reg_addr_type addr_type;
	enum msm_camera_i2c_data_type     data_type;
	uint16_t                          delay;
};

/* ---- the library struct, offsets as recovered above ---- */

struct flash_lib_t {
	void                                  *self;              /* +0x000 */
	char                                   flash_name[32];    /* +0x004 */
	enum msm_flash_driver_type             flash_driver_type; /* +0x024 */
	struct msm_sensor_power_setting_array  power_setting;     /* +0x028 */
	uint32_t                               slave_addr;        /* +0x578 */
	uint32_t                               unknown_57c;       /* +0x57c */
	uint32_t                               unknown_580;       /* +0x580 */
	struct msm_camera_i2c_reg_setting_array init_setting;     /* +0x584 */
	struct msm_camera_i2c_reg_setting_array off_setting;      /* +0x5f4 */
	struct msm_camera_i2c_reg_setting_array low_setting;      /* +0x664 */
	struct msm_camera_i2c_reg_setting_array high_setting;     /* +0x6d4 */
	uint32_t                               unknown_744;       /* +0x744 */
	uint32_t                               flash_max_current[MAX_LED_TRIGGERS];   /* +0x748 */
	uint32_t                               torch_max_current[MAX_LED_TRIGGERS];   /* +0x754 */
	uint32_t                               flash_max_duration[MAX_LED_TRIGGERS];  /* +0x760 */
	uint32_t                               tail_pad;          /* +0x76c */
};

/*
 * These are the assertions that make the file trustworthy: every offset the
 * daemon loads, pinned.  If a field is ever inserted or a type widened, the
 * build fails here rather than the flash failing on the device.
 */
_Static_assert(sizeof(struct msm_sensor_power_setting) == 56, "power setting");
_Static_assert(sizeof(struct msm_sensor_power_setting_array) == 0x550, "power array");
_Static_assert(sizeof(struct msm_camera_i2c_reg_setting_array) == 0x70, "reg array");
_Static_assert(__builtin_offsetof(struct flash_lib_t, flash_name) == 0x004, "name");
_Static_assert(__builtin_offsetof(struct flash_lib_t, flash_driver_type) == 0x024, "type");
_Static_assert(__builtin_offsetof(struct flash_lib_t, power_setting) == 0x028, "power");
_Static_assert(__builtin_offsetof(struct flash_lib_t, slave_addr) == 0x578, "slave");
_Static_assert(__builtin_offsetof(struct flash_lib_t, init_setting) == 0x584, "init");
_Static_assert(__builtin_offsetof(struct flash_lib_t, off_setting) == 0x5f4, "off");
_Static_assert(__builtin_offsetof(struct flash_lib_t, low_setting) == 0x664, "low");
_Static_assert(__builtin_offsetof(struct flash_lib_t, high_setting) == 0x6d4, "high");
_Static_assert(__builtin_offsetof(struct flash_lib_t, flash_max_current) == 0x748, "current");
_Static_assert(__builtin_offsetof(struct flash_lib_t, flash_max_duration) == 0x760, "duration");
_Static_assert(sizeof(struct flash_lib_t) == 0x770, "flash_lib_t size");

/* ---- Hill ---- */

#define HILL_SLAVE_ADDR   0x60      /* 8-bit; the kernel does sid = addr >> 1 */

#define HILL_REG_CURRENT1 0x02
#define HILL_REG_CURRENT2 0x03
#define HILL_REG_CURRENT3 0x04
#define HILL_REG_MODE     0x05
#define HILL_REG_DROOP    0x06
#define HILL_REG_DURATION 0x07
#define HILL_REG_ENABLE   0x08

#define HILL_MODE_NORMAL    0x19
#define HILL_MODE_POWER_ON  0x00
#define HILL_MODE_POWER_OFF 0x04

#define HILL_EN_STANDBY   0x03
#define HILL_EN_TORCH     0x06
#define HILL_EN_FLASH     0x07

/*
 * THE MAIN FLASH FIRES IN FLASH MODE -- but only when flash mode is entered
 * from TORCH, never from standby.
 *
 * Hill's boost is inductive and flash mode does not start it.  Arming the
 * part cold -- 0x08: 0x03 -> 0x07, which is what the register map says and
 * what Windows writes -- leaves it armed and dark: the enable reads back
 * 0x07 and KEEPS reading 0x07, measured past the 124 ms its duration code
 * implies, past 8 ms at duration code 0x02, and again ten seconds later.
 * Entering flash from torch, where the boost is already running, strobes
 * every time.  Observed directly on the LEDs: 0 of 4 cold, 4 of 4 and 5 of 5
 * pre-charged, twice, at 10 ms and 100 ms of dwell.
 *
 * ⚠ This is why S153 concluded "Hill's flash mode never strobes on this
 * board" and shipped the main flash in torch mode instead.  Every arm in
 * that sweep went 0x03 -> 0x07, so every variant it tried -- plain 0x07,
 * 0xE7, mode bytes 0x19/0xD9/0x99, CAM_FLASH_EN high -- measured 0.0 for the
 * same single reason.  The registers were never wrong.  Retracted; the
 * extra registers that sweep never reached (0x0e, 0x22, droop 0x06) were
 * tested here too and are irrelevant: 4 of 4 without any of them.
 *
 * ⚠ AND THE ENABLE REGISTER IS NOT A STATUS SIGNAL.  S153 read "it still
 * reads 0x07 after the burst, where a strobe that had run would have
 * returned it to standby" as proof the part never triggered.  It proves
 * nothing: 0x08 holds 0x07 whether or not the part strobed, and never
 * self-reverts at any duration code.  The light is a brief pulse fired on
 * the 0x06 -> 0x07 transition; the register stays armed long after the LEDs
 * are dark.  Confirmed with fixed-exposure RAW: flash mode held on for a
 * whole multi-second burst reads 16.2/16.7/16.7 against an ambient of
 * 16.0/15.7/16.1, while torch held the same way reads 26.  Judge this part
 * by the LEDs or by a synchronised capture -- never by 0x08.
 *
 * Consequence for the safety timer: there isn't one that works, so the
 * software timeout in msm_flash.c (`flash_high_timeout_ms`, S182) is not a
 * stand-in for absent hardware -- it is the only thing that returns the part
 * to standby, and it stays.
 */

/*
 * How long the boost needs before flash mode will trigger.  10 ms is
 * measured (4 of 4); 100 ms also works and back-to-back writes with no dwell
 * at all do not.  The exact floor between them is not established, so this
 * is the proven value and not a tuned one.
 *
 * Expressed in the register array's own `delay` field, in microseconds --
 * msm_flash_i2c_write_table() splits the array there and sleeps.
 */
#ifndef HILL_PRECHARGE_US
#define HILL_PRECHARGE_US 10000
#endif

/*
 * Hill's current is not a mA value on this part: it is one of eight levels in
 * the top three bits of each current register, and the low five bits are
 * inert.  So an operating point is compiled in here rather than chosen at
 * runtime -- with FLASH_DRIVER_I2C the kernel writes these arrays verbatim and
 * ignores the currents the HAL asks for, so there is nowhere for a runtime
 * brightness to go on this path.  Change these two and rebuild.
 * (sensor/flash/hill.c keeps its own runtime torch_level / flash_level params
 * for /sys/class/leds/flash-torch.)
 *
 * ⚠ Windows disagrees and the part wins -- do not "fix" these to Windows'
 * numbers.  Hill_SetFlash (qccamflash8992.sys 0x00409b10) builds every current
 * as `mA / 50`, divided explicitly at 0x004086b8 and limit-checked at
 * 0x00408504 to 0x18 per LED and 0x30 in total, so Windows only ever writes
 * 0x00..0x18.  Measured on this phone every one of those emits nothing: 0x0a
 * and 0x18 read back byte-identical to the LED off, 0x20/0x2f/0x3f are
 * identical to each other as are 0x40/0x5f, and in flash mode 0x18 and 0x0d
 * make the part abort to standby 0x03 while 0xe0 stays armed at 0x07.  The
 * conflict is unexplained; the measurement is what ships.  Evidence:
 * camera-lab/hill-current-encoding.sh.
 */
#define HILL_CUR(level)   ((uint8_t)((level) << 5))
/*
 * Overridable from the build line (-DHILL_TORCH_LEVEL=n) so the preflash level
 * can be swept without editing this file.  The preflash has two constraints
 * that pull against each other: it must be bright enough for AEC to MEASURE a
 * luma change during precapture, and its ratio to the main flash must match the
 * chromatix `flux_ratio` AEC extrapolates with (imx377's, 7.0).  Level 2 was
 * chosen in S71 to satisfy the second; S147 found it may fail the first.
 */
#ifndef HILL_TORCH_LEVEL
#define HILL_TORCH_LEVEL  2
#endif
#ifndef HILL_FLASH_LEVEL
#define HILL_FLASH_LEVEL  7
#endif

/*
 * 0x07 is a safety timer, not a duration: Windows computes
 * `code = (ms - 4) / 4 + 1`, clamped to 0x1f (~128 ms) and 0 below 5 ms
 * (qccamflash8992.sys 0x00408450).  This was 0xff, which is not a legal code
 * at all -- the field is five bits, so 0xff was also setting three unknown
 * bits above it.  Nothing on this path passes a duration, so use 100 ms, a
 * backstop well clear of the ~38 ms full-resolution readout.
 */
#define HILL_MS_TO_CODE(ms)  ((ms) < 5 ? 0 : (uint8_t)((((ms) - 4) / 4) + 1))
#define HILL_FLASH_DUR    HILL_MS_TO_CODE(100)   /* 0x19 */

/*
 * Why the preflash is level 2 and not level 4.
 *
 * AEC never measures the main flash.  It measures the scene once with the
 * preflash (CFG_FLASH_LOW, this low_setting), producing si_low, and then
 * *assumes* the main flash is `flux_ratio` times brighter -- flux_ratio is a
 * chromatix constant and imx377's says 7.0:
 *
 *   aec_process_estimate_led_bias_table: max_flash_on(14620), max_flash_off(28),
 *                                        flash_high_max(102172)   <- 14620 * 7
 *   aec_process_led_adjust_exp_settings: si_off(4529664), si_low(37596),
 *                                        si_hi(5031), flux_ratio(7.000000)
 *   aec_process_led_adjust_exp_settings: adj(-226) ==> final exp_index(96)
 *
 * So if the hardware's real high/low ratio is smaller than flux_ratio, AEC
 * over-estimates the main flash and cuts the snapshot exposure too far.  With
 * the preflash at level 4 it did exactly that: the flash shot came out at 16
 * lines, 1/6175 s, against 1/60 s for the same scene with the flash off, i.e.
 * about three stops dark, which is what "the images are way too dark with the
 * flash on" was.
 *
 * Hill's current is a level in bits [7:5], not a mA value, and its flux is
 * strongly non-linear in that level.  Measured on this sensor with
 * camera-lab/hill-flux-curve.sh -- AEC is settled at each level and the
 * illumination read off as cur_luma/(real_gain*linecnt), ambient subtracted:
 *
 *   level    1       2       3       4       5       6       7
 *   flux   0.089   0.140   0.183   0.230   0.324   0.498   0.884
 *
 *   level 4 -> 7 : 3.85     <- what we shipped, against a tuning that wants 7
 *   level 2 -> 7 : 6.33     <- within 10% of flux_ratio
 *   level 1 -> 7 : 9.90
 *
 * Level 2 is therefore the preflash this tuning is written for.  It also stops
 * the metering frame clipping: at level 4 the LED-on preflash frame came back
 * at luma 251 of 255, so the ratio AEC divided by was measured against a
 * saturated frame.
 *
 * This dims the in-camera video torch, which shares CFG_FLASH_LOW.  The
 * flashlight tile does not go this way -- it has no userspace component and
 * falls back to qcom,flash-low-setting in the device tree, which stays at
 * level 4.  See msm_flash_config()'s dt_setting fallback.
 */

/*
 * Order is Windows' own: currents, mode, droop, duration, then enable --
 * enable last, so the part never runs with a stale current.
 */
#define HILL_ON_SEQUENCE(level, duration, enable)                        \
	{                                                                \
		{ HILL_REG_CURRENT1, HILL_CUR(level), 0 },               \
		{ HILL_REG_CURRENT2, HILL_CUR(level), 0 },               \
		{ HILL_REG_CURRENT3, HILL_CUR(level), 0 },               \
		{ HILL_REG_MODE,     HILL_MODE_NORMAL, 0 },              \
		{ HILL_REG_DROOP,    0, 0 },                             \
		{ HILL_REG_DURATION, (duration), 0 },                    \
		{ HILL_REG_ENABLE,   (enable), 0 },                      \
	}

/*
 * WHICH MODE THE MAIN FLASH USES.  Default 0 = torch.
 *
 * Flash mode genuinely works now (see above) -- but a flash-mode strobe does
 * not expose the picture on this port, and that is a synchronisation problem
 * rather than a register one.  Measured, kernel #4, Camera1, one scene,
 * interleaved:
 *
 *            ISO   mean          CFG_FLASH_HIGH reaching the kernel
 *   off      559   74.7 / 80.0
 *   flash    454   51.4 / 46.4   yes, rc 0, ioctl taking 14.6 ms (the dwell)
 *
 * i.e. the flash shot came out DARKER than flash-off, because AEC lowered the
 * sensitivity expecting `flux_ratio` times more light and the strobe was over
 * before the snapshot frame was exposed.  The pulse is brief and it fires
 * ~15 ms into the CFG_FLASH_HIGH ioctl, while the snapshot arrives tens of ms
 * later; nothing in this path can move it.  Sustained torch light covers the
 * snapshot whenever it lands, which is why torch is still what ships.
 *
 * What would make flash mode usable is a hardware trigger at exposure time.
 * Worth investigating before choosing this: SMIA++ sensors have a flash
 * strobe output, and Hill's mode byte 0x05 -- which Windows writes as three
 * different values, 0x19 / 0xD9 / 0x99 -- may select an external strobe
 * input over software triggering.  GPIO 12 is NOT that trigger (torch lights
 * identically with it high or low, and ACPI declares FLSH with no resources).
 */
#ifndef HILL_USE_FLASH_MODE
#define HILL_USE_FLASH_MODE 0
#endif

/*
 * The main flash.  Currents are parked at zero for the pre-charge so the
 * torch step emits no light of its own, then raised before the part is moved
 * to flash mode:
 *
 *   currents 0 -> mode/droop/duration -> ENABLE=torch, dwell -> currents up
 *   -> ENABLE=flash
 *
 * Eleven entries against MAX_I2C_REG_SET's twelve.  That is the reason the
 * currents are zeroed rather than 0x0e/0x22 being written as well -- those
 * would not fit, and they were measured to make no difference.
 */
/*
 * THE CURRENT REGISTERS ARE SPLIT FIELDS -- bits [7:5] torch level, bits
 * [4:0] flash current in units of 50 mA.  Decoded from Hill_SetFlash
 * (qccamflash8992.sys 0x00409b10), which writes, for LED currents r4/r5/r6
 * expressed as Windows' mA/50 counts:
 *
 *     reg 0x1e = r6*5   reg 0x1f = r5*5   reg 0x20 = r4*5     (10 mA units)
 *     reg 0x02 = r6     reg 0x03 = r5     reg 0x04 = r4       (the counts)
 *     reg 0x05 = 0x19 / 0xd9 / 0x99                           (mode)
 *     total_mA = (r4 + r5 + r6) * 0x32
 *
 * ⚠ This retires the conflict CLAUDE.md carried as "unexplained" since S108:
 * "Windows only ever writes 0x00..0x18 and on this part those emit nothing,
 * while only bits [7:5] carry current".  Both observations were right and the
 * conclusion drawn from them was wrong.  0x18 is a FLASH current with torch
 * level 0, so of course it emits nothing in torch mode; 0xe0 is torch level 7
 * with **flash current 0**.
 *
 * Which means every flash-mode strobe fired before this ran at ZERO flash
 * current -- the part had nothing to strobe with.  That, not the trigger, is
 * the likeliest reason the pulse was too brief and too dim for a
 * fixed-exposure RAW burst to see.
 *
 * Windows limit-checks to 0x18 per LED and 0x30 in total (0x00408504), so
 * three LEDs share 48 counts: 0x10 each = 800 mA each, 2400 mA total, right
 * at the cap.
 */
#ifndef HILL_FLASH_COUNT
#define HILL_FLASH_COUNT 0x10
#endif

/*
 * Droop compensation, register 0x06.
 *
 * ⚠ This was 0 because S187 measured it "irrelevant" -- 4 of 4 strobes fired
 * without it.  That test is void: it ran with the flash-current field at zero
 * (0xe0 = torch level 7, flash current 0, see S188), so the part was not
 * drawing flash current at all and had nothing to compensate for.  With the
 * current field populated the pulse collapses almost immediately -- the enable
 * reads back 0x03 within ~15 ms of being set to 0x07 -- which is what a part
 * aborting on supply droop does.
 *
 * 0x5e is the value every visible strobe in S187 was fired with.
 */
#ifndef HILL_FLASH_DROOP
#define HILL_FLASH_DROOP 0x5e
#endif
#define HILL_FLASH_CUR(level) \
	((uint8_t)((((level) & 7) << 5) | (HILL_FLASH_COUNT & 0x1f)))
#define HILL_FLASH_CUR10      ((uint8_t)(HILL_FLASH_COUNT * 5))

#define HILL_REG_FCUR3    0x1e
#define HILL_REG_FCUR2    0x1f
#define HILL_REG_FCUR1    0x20

/*
 * The main flash: Windows' current programming, then the boost pre-charge
 * this part needs before flash mode will trigger at all.
 *
 * Eleven entries against MAX_I2C_REG_SET's twelve.  The currents are written
 * once, before the pre-charge, rather than parked at zero and raised after --
 * the torch field is in the same register, so a dark pre-charge would cost
 * three more entries and overflow.  The cost is 10 ms of torch light ahead of
 * the strobe, which is below the shutter lag anyway.
 */
#define HILL_FLASH_SEQUENCE(level, duration)                             \
	{                                                                \
		{ HILL_REG_ENABLE,   HILL_EN_STANDBY, 0 },               \
		{ HILL_REG_FCUR3,    HILL_FLASH_CUR10, 0 },              \
		{ HILL_REG_FCUR2,    HILL_FLASH_CUR10, 0 },              \
		{ HILL_REG_FCUR1,    HILL_FLASH_CUR10, 0 },              \
		{ HILL_REG_CURRENT1, HILL_FLASH_CUR(level), 0 },         \
		{ HILL_REG_CURRENT2, HILL_FLASH_CUR(level), 0 },         \
		{ HILL_REG_CURRENT3, HILL_FLASH_CUR(level), 0 },         \
		{ HILL_REG_MODE,     HILL_MODE_NORMAL, 0 },              \
		{ HILL_REG_DROOP,    HILL_FLASH_DROOP, 0 },              \
		{ HILL_REG_DURATION, (duration), 0 },                    \
		{ HILL_REG_ENABLE,   HILL_EN_TORCH, HILL_PRECHARGE_US }, \
		{ HILL_REG_ENABLE,   HILL_EN_FLASH, 0 },                 \
	}
/*
 * Twelve entries, exactly MAX_I2C_REG_SET.  It opens with STANDBY so that
 * msm_flash.c's flash_high_repeat_ms can replay the whole array and get a
 * fresh 0x03 -> 0x06 -> 0x07 edge each time: flash mode only triggers on that
 * transition, and re-writing 0x07 while already at 0x07 does nothing.
 */

static struct flash_lib_t hill_flash_lib = {
	.flash_name        = "hill",
	.flash_driver_type = FLASH_DRIVER_I2C,
	.slave_addr        = HILL_SLAVE_ADDR,

	/*
	 * msm_flash_i2c_init refuses a NULL power_setting_array and demands
	 * 1..MAX_POWER_CONFIG entries either way, so the sequence cannot be
	 * empty.  GPIO 12 is the flash enable the DT names
	 * (qcom,gpio-flash-en = <0>), and the part is verified to light with
	 * that pin LOW -- toggling it alone lights nothing -- so this drives
	 * it to the state the working measurements were taken in rather than
	 * guessing high.
	 */
	.power_setting = {
		.power_setting_a = {
			{ .seq_type = SENSOR_GPIO, .seq_val = SENSOR_GPIO_FL_EN,
			  .config_val = GPIO_OUT_LOW, .delay = 1 },
		},
		.size = 1,
		.power_down_setting_a = {
			{ .seq_type = SENSOR_GPIO, .seq_val = SENSOR_GPIO_FL_EN,
			  .config_val = GPIO_OUT_LOW, .delay = 1 },
		},
		.size_down = 1,
	},

	/* Hill is byte-addressed with byte data. */
	.init_setting = {
		.reg_setting_a = { { HILL_REG_MODE, HILL_MODE_POWER_ON, 0 } },
		.size = 1,
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},
	.off_setting = {
		.reg_setting_a = { { HILL_REG_ENABLE, HILL_EN_STANDBY, 0 } },
		.size = 1,
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},
	.low_setting = {
		.reg_setting_a =
			HILL_ON_SEQUENCE(HILL_TORCH_LEVEL, 0, HILL_EN_TORCH),
		.size = 7,
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},
	.high_setting = {
#if HILL_USE_FLASH_MODE
		/*
		 * A real flash-mode strobe, pre-charged.  ⚠ MEASURED NOT TO
		 * EXPOSE THE SNAPSHOT on this port -- see HILL_USE_FLASH_MODE
		 * above.  Kept because the sequence is correct and the moment
		 * it can be triggered at exposure time it is what we want.
		 */
		.reg_setting_a = HILL_FLASH_SEQUENCE(HILL_FLASH_LEVEL,
						     HILL_FLASH_DUR),
		.size = 12,
#else
		/*
		 * Torch mode, duration 0: the safety timer only counts in
		 * flash mode, and (measured) does not clear the enable even
		 * there, so msm_flash.c's flash_high_timeout_ms is what
		 * switches the part off after a capture -- see S182.
		 */
		.reg_setting_a = HILL_ON_SEQUENCE(HILL_FLASH_LEVEL, 0,
						  HILL_EN_TORCH),
		.size = 7,
#endif
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},

	/*
	 * What the stack advertises upward, per LED.  These are the ceilings
	 * the old DT node reported (qcom,max-current / qcom,max-duration) and
	 * they only reach the HAL's capability numbers -- FLASH_DRIVER_I2C
	 * never turns them into register writes.  torch_max_current is at the
	 * one offset in this struct that no observed load reads, so it is a
	 * reasonable guess rather than a recovered fact.
	 */
	.flash_max_current  = { 1000, 1000, 1000 },
	.torch_max_current  = {  200,  200,  200 },
	.flash_max_duration = { 1280, 1280, 1280 },
};

/* The reference lib carries a self-pointer here; nothing observed reads it. */
static void *const hill_self = &hill_flash_lib;

void *flash_driver_open_lib(void)
{
	hill_flash_lib.self = (void *)hill_self;
	return &hill_flash_lib;
}
