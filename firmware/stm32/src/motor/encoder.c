/*
 * encoder.c — Software quadrature decoder via EXTI interrupts.
 * Motor 1: PC4 (A), PC5 (B)
 * Motor 2: PB9 (A), PB8 (B)
 *
 * Uses EXTI on channel A (both edges). Reads channel B to determine direction.
 */

#ifndef NATIVE_TEST

#include "encoder.h"
#include "pin_config.h"
#include "robot_params.h"
#include "stm32f4xx_hal.h"
#include <math.h>

static volatile int32_t enc_count[2] = {0, 0};

void encoder_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Motor 1 encoder A: PC4 — EXTI (both edges) */
    gpio.Pin  = MOTOR1_ENC_A_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MOTOR1_ENC_A_PORT, &gpio);

    /* Motor 1 encoder B: PC5 — input (read in ISR) */
    gpio.Pin  = MOTOR1_ENC_B_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(MOTOR1_ENC_B_PORT, &gpio);

    /* Motor 2 encoder A: PB9 — EXTI (both edges) */
    gpio.Pin  = MOTOR2_ENC_A_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    HAL_GPIO_Init(MOTOR2_ENC_A_PORT, &gpio);

    /* Motor 2 encoder B: PB8 — input (read in ISR) */
    gpio.Pin  = MOTOR2_ENC_B_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(MOTOR2_ENC_B_PORT, &gpio);

    /* Enable EXTI interrupts */
    HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);      /* PC4 */
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);     /* PB9 */
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

int32_t encoder_read_and_reset(encoder_id_t id)
{
    /* Atomic read-and-clear using interrupt disable */
    __disable_irq();
    int32_t val = enc_count[id];
    enc_count[id] = 0;
    __enable_irq();
    return val;
}

int32_t encoder_read(encoder_id_t id)
{
    return enc_count[id];
}

float encoder_ticks_to_metres(int32_t ticks)
{
    return (float)ticks * (2.0f * (float)M_PI * WHEEL_RADIUS_M) / (float)ENCODER_CPR;
}

/* ── EXTI ISR callbacks ─────────────────────────────────────── */

void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(MOTOR1_ENC_A_PIN);
}

void EXTI9_5_IRQHandler(void)
{
    /* This line handles both PB9 (encoder) and PC5 if triggered */
    HAL_GPIO_EXTI_IRQHandler(MOTOR2_ENC_A_PIN);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MOTOR1_ENC_A_PIN) {
        /* Read A and B states */
        uint8_t a = HAL_GPIO_ReadPin(MOTOR1_ENC_A_PORT, MOTOR1_ENC_A_PIN);
        uint8_t b = HAL_GPIO_ReadPin(MOTOR1_ENC_B_PORT, MOTOR1_ENC_B_PIN);
        enc_count[ENCODER_LEFT] += (a == b) ? 1 : -1;
    }

    if (GPIO_Pin == MOTOR2_ENC_A_PIN) {
        uint8_t a = HAL_GPIO_ReadPin(MOTOR2_ENC_A_PORT, MOTOR2_ENC_A_PIN);
        uint8_t b = HAL_GPIO_ReadPin(MOTOR2_ENC_B_PORT, MOTOR2_ENC_B_PIN);
        enc_count[ENCODER_RIGHT] += (a == b) ? 1 : -1;
    }
}

#endif /* !NATIVE_TEST */
