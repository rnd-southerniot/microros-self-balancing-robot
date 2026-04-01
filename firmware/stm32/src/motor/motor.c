/*
 * motor.c — Dual H-bridge motor driver.
 * Motor 1: TIM3_CH1 (PB4), direction PA5/PA7
 * Motor 2: TIM4_CH2 (PB7), direction PD4/PD5
 * PWM frequency: 20 kHz (inaudible)
 */

#ifndef NATIVE_TEST

#include "motor.h"
#include "pin_config.h"
#include "robot_params.h"
#include "stm32f4xx_hal.h"

static TIM_HandleTypeDef htim3;  /* Motor 1 PWM */
static TIM_HandleTypeDef htim4;  /* Motor 2 PWM */

/* PWM period for 20 kHz at 90 MHz timer clock: 90M/20k = 4500 */
#define PWM_PERIOD  4499

static float clampf(float val, float limit)
{
    if (val >  limit) return  limit;
    if (val < -limit) return -limit;
    return val;
}

void motor_init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* ── TIM3 CH1: Motor 1 PWM on PB4 ── */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = MOTOR1_PWM_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = MOTOR1_PWM_AF;
    HAL_GPIO_Init(MOTOR1_PWM_PORT, &gpio);

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = PWM_PERIOD;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    /* ── TIM4 CH2: Motor 2 PWM on PB7 ── */
    gpio.Pin       = MOTOR2_PWM_PIN;
    gpio.Alternate = MOTOR2_PWM_AF;
    HAL_GPIO_Init(MOTOR2_PWM_PORT, &gpio);

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 0;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = PWM_PERIOD;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim4);

    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
}

void motor_set(motor_id_t id, float duty)
{
    duty = clampf(duty, MAX_MOTOR_DUTY);

    GPIO_TypeDef *dir_a_port, *dir_b_port;
    uint16_t dir_a_pin, dir_b_pin;
    TIM_HandleTypeDef *htim;
    uint32_t channel;

    if (id == MOTOR_LEFT) {
        dir_a_port = MOTOR1_DIR_A_PORT;
        dir_a_pin  = MOTOR1_DIR_A_PIN;
        dir_b_port = MOTOR1_DIR_B_PORT;
        dir_b_pin  = MOTOR1_DIR_B_PIN;
        htim       = &htim3;
        channel    = TIM_CHANNEL_1;
    } else {
        dir_a_port = MOTOR2_DIR_A_PORT;
        dir_a_pin  = MOTOR2_DIR_A_PIN;
        dir_b_port = MOTOR2_DIR_B_PORT;
        dir_b_pin  = MOTOR2_DIR_B_PIN;
        htim       = &htim4;
        channel    = TIM_CHANNEL_2;
    }

    /* Set direction */
    if (duty >= 0.0f) {
        HAL_GPIO_WritePin(dir_a_port, dir_a_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(dir_b_port, dir_b_pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(dir_a_port, dir_a_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(dir_b_port, dir_b_pin, GPIO_PIN_SET);
        duty = -duty;
    }

    /* Set PWM duty */
    uint32_t pulse = (uint32_t)(duty * (float)(PWM_PERIOD + 1));
    __HAL_TIM_SET_COMPARE(htim, channel, pulse);
}

void motor_stop_all(void)
{
    /* Zero PWM */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);

    /* Brake: both direction pins LOW */
    HAL_GPIO_WritePin(MOTOR1_DIR_A_PORT, MOTOR1_DIR_A_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR1_DIR_B_PORT, MOTOR1_DIR_B_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR2_DIR_A_PORT, MOTOR2_DIR_A_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR2_DIR_B_PORT, MOTOR2_DIR_B_PIN, GPIO_PIN_RESET);
}

#endif /* !NATIVE_TEST */
