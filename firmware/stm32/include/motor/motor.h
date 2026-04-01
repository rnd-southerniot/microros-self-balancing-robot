#pragma once

/*
 * motor.h
 * Dual H-bridge motor driver via PWM + GPIO direction.
 * Motor 1: TIM3_CH1 (PB4), dir PA5/PA7
 * Motor 2: TIM4_CH2 (PB7), dir PD4/PD5
 */

#include <stdint.h>

typedef enum {
    MOTOR_LEFT  = 0,
    MOTOR_RIGHT = 1,
} motor_id_t;

/**
 * @brief  Initialise PWM timers and direction GPIO for both motors.
 */
void motor_init(void);

/**
 * @brief  Set motor duty cycle. Positive = forward, negative = reverse.
 * @param  id    MOTOR_LEFT or MOTOR_RIGHT
 * @param  duty  [-1.0 .. +1.0], clamped to MAX_MOTOR_DUTY
 */
void motor_set(motor_id_t id, float duty);

/**
 * @brief  Emergency stop — zero PWM, brake both motors.
 */
void motor_stop_all(void);
