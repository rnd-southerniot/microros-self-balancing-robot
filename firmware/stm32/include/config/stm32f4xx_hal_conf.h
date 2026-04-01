/*
 * stm32f4xx_hal_conf.h
 * HAL module configuration for STM32F429I-DISC1.
 * Enable only the modules we actually use.
 */

#pragma once

/* ── Enabled HAL modules ────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED

/* ── Oscillator values ──────────────────────────────────────── */
#ifndef HSE_VALUE
#define HSE_VALUE    8000000U   /* 8 MHz crystal on Discovery */
#endif

#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT  100U
#endif

#ifndef HSI_VALUE
#define HSI_VALUE    16000000U
#endif

#ifndef LSI_VALUE
#define LSI_VALUE    32000U
#endif

#ifndef LSE_VALUE
#define LSE_VALUE    32768U
#endif

#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT  5000U
#endif

#ifndef EXTERNAL_CLOCK_VALUE
#define EXTERNAL_CLOCK_VALUE  12288000U
#endif

/* ── System configuration ───────────────────────────────────── */
#define VDD_VALUE                    3300U    /* mV */
#define TICK_INT_PRIORITY            0x0FU
#define USE_RTOS                     0U
#define PREFETCH_ENABLE              1U
#define ART_ACCELERATOR_ENABLE       1U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

/* ── HAL module includes ────────────────────────────────────── */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32f4xx_hal_exti.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32f4xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32f4xx_hal_pwr.h"
#endif
#ifdef HAL_I2C_MODULE_ENABLED
  #include "stm32f4xx_hal_i2c.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32f4xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32f4xx_hal_uart.h"
#endif

/* ── Assert macro ───────────────────────────────────────────── */
/* #define USE_FULL_ASSERT  1U */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif
