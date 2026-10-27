// SPDX-License-Identifier: GPL-2.0-only
/*
 * AF algorithm tuning for the Carrera actuator.
 *
 * There is no Lumia AF tuning in Android format. As a first, explicitly
 * provisional step this forwards to the vendor lc898212xd tuning
 * (Nexus 5X optics), whose step indexes use the same 325-step scale as
 * actuator_carrera.c. Replace with real tuning once AF moves are verified.
 *
 * Built twice: CARRERA_AF_MODE=camera and CARRERA_AF_MODE=camcorder.
 */
#include <dlfcn.h>
#include <stddef.h>

#define STR2(x) #x
#define STR(x) STR2(x)
#define CAT4_(a, b, c, d) a##b##c##d
#define CAT4(a, b, c, d) CAT4_(a, b, c, d)

#ifndef CARRERA_AF_SYMBOL_NAME
#error "CARRERA_AF_SYMBOL_NAME must be the actuator name used by the sensor module"
#endif

#define REF_LIB "libactuator_lc898212xd_" STR(CARRERA_AF_MODE) ".so"
#define REF_SYM "lc898212xd_" STR(CARRERA_AF_MODE) "_af_algo_open_lib"

__attribute__((visibility("default")))
void *CAT4(CARRERA_AF_SYMBOL_NAME, _, CARRERA_AF_MODE, _af_algo_open_lib)(void)
{
    static void *handle;
    void *(*open_ref)(void);

    if (!handle)
        handle = dlopen(REF_LIB, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return NULL;
    open_ref = (void *(*)(void))dlsym(handle, REF_SYM);
    return open_ref ? open_ref() : NULL;
}
