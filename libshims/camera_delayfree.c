/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Preloaded only into mm-qcamera-daemon.
 *
 * The angler AF port (libmmcamera2_stats_modules.so,
 * af_port_handle_set_parm_event) queues a message to the AF thread and then
 * reads msg->sync_flag; for asynchronous messages the AF thread may already
 * have freed it. With the Android 8 allocator the memory stayed mapped; with
 * scudo a large block is unmapped at once and the daemon faults as soon as
 * an AF actuator is present. Keep the last frees pending in a small ring so
 * such late reads still hit valid memory.
 */
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>

#define DELAY_SLOTS 512

static void (*real_free)(void *);
static void *ring[DELAY_SLOTS];
static unsigned int ring_pos;
static pthread_mutex_t ring_lock = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor)) static void delayfree_init(void)
{
    real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
}

__attribute__((visibility("default"))) void free(void *ptr)
{
    void *old;

    if (!ptr)
        return;
    if (!real_free) {
        real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
        if (!real_free)
            return; /* leak rather than crash during early init */
    }
    pthread_mutex_lock(&ring_lock);
    old = ring[ring_pos];
    ring[ring_pos] = ptr;
    ring_pos = (ring_pos + 1) % DELAY_SLOTS;
    pthread_mutex_unlock(&ring_lock);
    if (old)
        real_free(old);
}
