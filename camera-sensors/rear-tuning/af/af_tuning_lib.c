/*
 * Taken from hraj9258's talkman port (Himanshu Raj):
 * gitlab.com/hraj9258/android_device_msft_talkman, lineage-18.1-talkman,
 * 901b039, camera/sensors/imx230/af/af_tuning_lib.c
 */
/* The talkman AF search tuning container.
 *
 * Compiled once per mode with -DAF_MODE and -DAF_DATA.  Reproduces the donor's
 * container exactly: a self-pointer and a zero word ahead of the struct, and an
 * open_lib() returning .data + 8.  See af_tuning_talkman.h.
 */
#include "af_tuning_structs.h"

#define AF_CAT2(a, b) a##b
#define AF_CAT(a, b)  AF_CAT2(a, b)
#define AF_OPEN_LIB   AF_CAT(AF_CAT(cityman_, AF_MODE), _af_algo_open_lib)

static struct af_container {
	void        *self;
	uint32_t     reserved;
	af_tuning_t  tuning;
} af_blob = {
	.self     = &af_blob,
	.reserved = 0,
	.tuning   = {
#include AF_DATA
	},
};

AF_STATIC_ASSERT(__builtin_offsetof(struct af_container, tuning) == 8,
                 container_tuning_at_8);

void *AF_OPEN_LIB(void);
void *AF_OPEN_LIB(void)
{
	return &af_blob.tuning;
}
