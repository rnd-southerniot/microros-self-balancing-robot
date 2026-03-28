#pragma once

/*
 * pin_config.h
 * All GPIO, timer channel, and peripheral bus assignments for
 * STM32F429I-DISC1.
 *
 * Confirmed from hardware validation (dual-motor-controller-balancer repo).
 * Do NOT change without updating the CubeMX config and re-validating.
 *
 * Reference: docs/hardware/pin-map.md
 */

/* ── Motor 1 ─────────────────────────────────────────────────── */
#define MOTOR1_PWM_PORT         GPIOB
#define MOTOR1_PWM_PIN          GPIO_PIN_4      /* PB4 → TIM3_CH1 AF2       */
#define MOTOR1_PWM_TIM          TIM3
#define MOTOR1_PWM_CHANNEL      TIM_CHANNEL_1
#define MOTOR1_PWM_AF           GPIO_AF2_TIM3

#define MOTOR1_DIR_A_PORT       GPIOA
#define MOTOR1_DIR_A_PIN        GPIO_PIN_5      /* PA5 → H-bridge IN1        */
#define MOTOR1_DIR_B_PORT       GPIOA
#define MOTOR1_DIR_B_PIN        GPIO_PIN_7      /* PA7 → H-bridge IN2        */

#define MOTOR1_ENC_A_PORT       GPIOC
#define MOTOR1_ENC_A_PIN        GPIO_PIN_4      /* PC4 → encoder ch A        */
#define MOTOR1_ENC_B_PORT       GPIOC
#define MOTOR1_ENC_B_PIN        GPIO_PIN_5      /* PC5 → encoder ch B        */
#define MOTOR1_ENC_TIM          TIM8            /* TODO: confirm encoder TIM */

/* ── Motor 2 ─────────────────────────────────────────────────── */
#define MOTOR2_PWM_PORT         GPIOB
#define MOTOR2_PWM_PIN          GPIO_PIN_7      /* PB7 → TIM4_CH2 AF2       */
#define MOTOR2_PWM_TIM          TIM4
#define MOTOR2_PWM_CHANNEL      TIM_CHANNEL_2
#define MOTOR2_PWM_AF           GPIO_AF2_TIM4

#define MOTOR2_DIR_A_PORT       GPIOD
#define MOTOR2_DIR_A_PIN        GPIO_PIN_4      /* PD4 → H-bridge IN1        */
#define MOTOR2_DIR_B_PORT       GPIOD
#define MOTOR2_DIR_B_PIN        GPIO_PIN_5      /* PD5 → H-bridge IN2        */

#define MOTOR2_ENC_A_PORT       GPIOB
#define MOTOR2_ENC_A_PIN        GPIO_PIN_9      /* PB9 → encoder ch A        */
#define MOTOR2_ENC_B_PORT       GPIOB
#define MOTOR2_ENC_B_PIN        GPIO_PIN_8      /* PB8 → encoder ch B        */
#define MOTOR2_ENC_TIM          TIM5            /* TODO: confirm encoder TIM */

/* ── IMU: ICM-20948 primary (I2C1) ──────────────────────────── */
#define ICM20948_I2C            I2C1
#define ICM20948_SCL_PORT       GPIOB
#define ICM20948_SCL_PIN        GPIO_PIN_6      /* PB6 → I2C1_SCL           */
#define ICM20948_SDA_PORT       GPIOB
#define ICM20948_SDA_PIN        GPIO_PIN_7      /* PB7 — NOTE: shared with  */
                                                /* MOTOR2 PWM on alt func.  */
                                                /* Verify no conflict in MX */

/* ── IMU: L3GD20 secondary (SPI1, onboard Discovery) ────────── */
#define L3GD20_SPI              SPI1
#define L3GD20_CS_PORT          GPIOC
#define L3GD20_CS_PIN           GPIO_PIN_1      /* PC1 → gyro CS             */
#define L3GD20_INT1_PORT        GPIOA
#define L3GD20_INT1_PIN         GPIO_PIN_1      /* PA1 → gyro INT1           */
#define L3GD20_INT2_PORT        GPIOB
#define L3GD20_INT2_PIN         GPIO_PIN_2      /* PB2 → DRDY                */

/* ── HC-SR04 Ultrasonic ──────────────────────────────────────── */
#define ULTRASONIC_TRIG_PORT    GPIOE
#define ULTRASONIC_TRIG_PIN     GPIO_PIN_9      /* TODO: confirm on chassis  */
#define ULTRASONIC_ECHO_PORT    GPIOE
#define ULTRASONIC_ECHO_PIN     GPIO_PIN_11     /* TODO: confirm on chassis  */
#define ULTRASONIC_TIM          TIM1            /* Input capture timer       */

/* ── UART: STM32 ↔ ESP32 ────────────────────────────────────── */
#define ESP32_UART              USART1
#define ESP32_UART_TX_PORT      GPIOA
#define ESP32_UART_TX_PIN       GPIO_PIN_9      /* PA9 → USART1_TX           */
#define ESP32_UART_RX_PORT      GPIOA
#define ESP32_UART_RX_PIN       GPIO_PIN_10     /* PA10 → USART1_RX          */

/* ── LCD ILI9341 (onboard Discovery, SPI5) ───────────────────── */
#define LCD_SPI                 SPI5
#define LCD_CS_PORT             GPIOC
#define LCD_CS_PIN              GPIO_PIN_2
#define LCD_DC_PORT             GPIOD
#define LCD_DC_PIN              GPIO_PIN_13
#define LCD_RST_PORT            GPIOD
#define LCD_RST_PIN             GPIO_PIN_12

/* ── User LED / Button (onboard Discovery) ───────────────────── */
#define LED_GREEN_PORT          GPIOG
#define LED_GREEN_PIN           GPIO_PIN_13
#define LED_RED_PORT            GPIOG
#define LED_RED_PIN             GPIO_PIN_14
#define USER_BUTTON_PORT        GPIOA
#define USER_BUTTON_PIN         GPIO_PIN_0      /* PA0 — wake-up button      */

/*
 * VALIDATION NOTES:
 * 1. PB7 is shared between I2C1_SDA (ICM-20948) and TIM4_CH2 (Motor2 PWM).
 *    These are alternate functions — only ONE can be active at a time.
 *    Motor2 PWM must use a different pin or ICM-20948 must use I2C3.
 *    ACTION: Resolve in CubeMX before Phase 1 hardware bring-up.
 *
 * 2. Encoder timer assignments (TIM8, TIM5) are placeholders.
 *    Confirm by checking which timers support encoder mode on F429
 *    and are not already used by PWM outputs.
 *    ACTION: Validate in CubeMX clock/pin view.
 *
 * 3. HC-SR04 pin assignments are placeholders.
 *    ACTION: Confirm physical wiring on chassis.
 */
