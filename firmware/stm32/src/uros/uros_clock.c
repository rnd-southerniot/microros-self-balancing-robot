/*
 * uros_clock.c — POSIX clock_gettime stub for microROS on bare-metal STM32.
 * Maps CLOCK_MONOTONIC/CLOCK_REALTIME to HAL_GetTick() (1ms resolution).
 */

#ifdef UROS_ENABLED

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* POSIX time types expected by microROS */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif

typedef int clockid_t;

struct timespec {
    int32_t  tv_sec;
    int32_t  tv_nsec;
};

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    (void)clk_id;
    uint32_t ms = HAL_GetTick();
    tp->tv_sec  = (int32_t)(ms / 1000);
    tp->tv_nsec = (int32_t)((ms % 1000) * 1000000);
    return 0;
}

#endif /* UROS_ENABLED */
