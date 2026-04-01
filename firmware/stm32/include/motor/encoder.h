#pragma once

/*
 * encoder.h
 * Software quadrature decoder using EXTI interrupts.
 * Motor 1: PC4 (A), PC5 (B)
 * Motor 2: PB9 (A), PB8 (B)
 *
 * These pins lack hardware timer encoder AF on STM32F429,
 * so we use EXTI rising/falling on channel A and read B for direction.
 */

#include <stdint.h>

typedef enum {
    ENCODER_LEFT  = 0,
    ENCODER_RIGHT = 1,
} encoder_id_t;

/**
 * @brief  Initialise EXTI interrupts for both encoders.
 */
void encoder_init(void);

/**
 * @brief  Get accumulated tick count (signed). Resets atomically.
 * @param  id  ENCODER_LEFT or ENCODER_RIGHT
 * @return Ticks since last call. Positive = forward.
 */
int32_t encoder_read_and_reset(encoder_id_t id);

/**
 * @brief  Get current tick count without resetting.
 */
int32_t encoder_read(encoder_id_t id);

/**
 * @brief  Convert ticks to metres travelled.
 */
float encoder_ticks_to_metres(int32_t ticks);
