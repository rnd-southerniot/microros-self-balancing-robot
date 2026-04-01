/*
 * main.c — MicroROS Self-Balancing Robot firmware
 *
 * STM32F429I-DISC1, bare-metal (timer interrupts for real-time).
 * 500 Hz IMU read, 200 Hz PID cascade, 50 Hz odometry.
 * microROS over USART1 → ESP32 WiFi bridge → ROS2 agent.
 */

#include "stm32f4xx_hal.h"
#include "pin_config.h"
#include "robot_params.h"
#include "icm20948.h"
#include "imu_filter.h"
#include "pid.h"
#include "motor.h"
#include "encoder.h"
#include <math.h>
#include <string.h>

#ifdef UROS_ENABLED
#include "uros_transport.h"
#include "uros_node.h"
#endif

/* ── Peripheral handles ─────────────────────────────────────── */
I2C_HandleTypeDef  hi2c3;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim6;   /* 1 kHz timebase for control scheduling */

/* ── Control state ──────────────────────────────────────────── */
static imu_filter_t   imu_filt;
static pid_t           pid_angle;
static pid_t           pid_vel;
static pid_t           pid_steer;
static icm20948_data_t imu_data;

static volatile uint32_t tick_1khz = 0;  /* incremented by TIM6 ISR */

static float cmd_vel_linear  = 0.0f;     /* from /cmd_vel subscriber  */
static float cmd_vel_angular = 0.0f;
static uint32_t cmd_vel_stamp = 0;       /* last cmd_vel receive time */

/* Odometry accumulators */
static float odom_x     = 0.0f;
static float odom_y     = 0.0f;
static float odom_theta = 0.0f;

/* Fault state */
static uint8_t fault_flags = 0;

/* ── Forward declarations ───────────────────────────────────── */
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C3_Init(void);
static void USART1_Init(void);
static void TIM6_Init(void);
static void LED_Set(uint8_t green, uint8_t red);

/* ── Main ───────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Peripheral init */
    GPIO_Init();
    I2C3_Init();
    USART1_Init();
    motor_init();
    encoder_init();
    TIM6_Init();

    /* IMU init */
    LED_Set(0, 1);  /* red = initialising */
    if (!icm20948_init()) {
        /* IMU not found — blink red forever */
        fault_flags |= IMU_FAULT_PRIMARY;
        while (1) {
            LED_Set(0, 1); HAL_Delay(200);
            LED_Set(0, 0); HAL_Delay(200);
        }
    }

    /* Filter init */
    imu_filter_init(&imu_filt, IMU_FILTER_ALPHA, 1.0f / IMU_SAMPLE_HZ);

    /* PID init */
    pid_init(&pid_angle, PID_ANGLE_KP, PID_ANGLE_KI, PID_ANGLE_KD,
             PID_ANGLE_IMAX, MAX_MOTOR_DUTY, 0.01f,
             1.0f / BALANCE_CTRL_HZ);

    pid_init(&pid_vel, PID_VEL_KP, PID_VEL_KI, PID_VEL_KD,
             PID_VEL_IMAX, 15.0f, 0.0f,
             1.0f / BALANCE_CTRL_HZ);

    pid_init(&pid_steer, PID_STEER_KP, PID_STEER_KI, PID_STEER_KD,
             PID_STEER_IMAX, MAX_MOTOR_DUTY * 0.5f, 0.0f,
             1.0f / BALANCE_CTRL_HZ);

    /* microROS init (if enabled) */
#ifdef UROS_ENABLED
    uros_transport_init();
    if (!uros_node_init()) {
        /* Agent not reachable — blink both LEDs */
        while (1) {
            LED_Set(1, 1); HAL_Delay(300);
            LED_Set(0, 0); HAL_Delay(300);
        }
    }
#endif

    /* Start timebase timer */
    HAL_TIM_Base_Start_IT(&htim6);

    LED_Set(1, 0);  /* green = running */

    /* ── Main loop ── */
    uint32_t last_imu_tick    = 0;
    uint32_t last_ctrl_tick   = 0;
    uint32_t last_odom_tick   = 0;

    while (1) {
        uint32_t now = tick_1khz;

        /* ── 500 Hz: IMU read + filter ── */
        if (now - last_imu_tick >= (1000 / IMU_SAMPLE_HZ)) {
            last_imu_tick = now;

            if (icm20948_read(&imu_data)) {
                imu_fault_t f = imu_filter_update(&imu_filt, &imu_data);
                if (f != IMU_OK) fault_flags |= f;
#ifdef UROS_ENABLED
                uros_publish_imu(&imu_data);
#endif
            } else {
                fault_flags |= IMU_FAULT_PRIMARY;
            }
        }

        /* ── 200 Hz: PID cascade ── */
        if (now - last_ctrl_tick >= (1000 / BALANCE_CTRL_HZ)) {
            last_ctrl_tick = now;

            float pitch = imu_filter_get_pitch(&imu_filt);

            /* Safety: e-stop on excessive tilt */
            if (fabsf(pitch - IMU_ZERO_ANGLE_DEG) > MAX_TILT_DEG) {
                motor_stop_all();
                pid_reset(&pid_angle);
                pid_reset(&pid_vel);
                pid_reset(&pid_steer);
                fault_flags |= 0x10;  /* tilt fault */
                continue;
            }

            /* cmd_vel: read from microROS subscriber or timeout */
#ifdef UROS_ENABLED
            uros_get_cmd_vel(&cmd_vel_linear, &cmd_vel_angular);
            cmd_vel_stamp = uros_get_cmd_vel_stamp();
#endif
            if (now - cmd_vel_stamp > CMD_VEL_TIMEOUT_MS) {
                cmd_vel_linear  = 0.0f;
                cmd_vel_angular = 0.0f;
            }

            /* Read encoders and compute velocity */
            int32_t enc_l = encoder_read_and_reset(ENCODER_LEFT);
            int32_t enc_r = encoder_read_and_reset(ENCODER_RIGHT);
            float dist_l = encoder_ticks_to_metres(enc_l);
            float dist_r = encoder_ticks_to_metres(enc_r);
            float velocity = (dist_l + dist_r) * 0.5f * BALANCE_CTRL_HZ;

            /* Outer loop: velocity → angle setpoint offset */
            float angle_offset = pid_compute(&pid_vel, cmd_vel_linear, velocity);

            /* Inner loop: angle → motor duty */
            float target_angle = IMU_ZERO_ANGLE_DEG + angle_offset;
            float motor_output = pid_compute(&pid_angle, target_angle, pitch);

            /* Steering: differential drive */
            float steer_output = pid_compute(&pid_steer, cmd_vel_angular, 0.0f);

            float duty_l = motor_output + steer_output;
            float duty_r = motor_output - steer_output;

            motor_set(MOTOR_LEFT,  duty_l);
            motor_set(MOTOR_RIGHT, duty_r);

            /* Clear tilt fault if recovered */
            fault_flags &= ~0x10;
        }

        /* ── 50 Hz: Odometry update ── */
        if (now - last_odom_tick >= (1000 / ODOM_PUB_HZ)) {
            last_odom_tick = now;

            int32_t enc_l = encoder_read_and_reset(ENCODER_LEFT);
            int32_t enc_r = encoder_read_and_reset(ENCODER_RIGHT);
            float dl = encoder_ticks_to_metres(enc_l);
            float dr = encoder_ticks_to_metres(enc_r);
            float dc = (dl + dr) * 0.5f;
            float dtheta = (dr - dl) / WHEEL_BASE_M;

            odom_theta += dtheta;
            odom_x += dc * cosf(odom_theta);
            odom_y += dc * sinf(odom_theta);

#ifdef UROS_ENABLED
            float vel_linear  = dc * ODOM_PUB_HZ;
            float vel_angular = dtheta * ODOM_PUB_HZ;
            uros_publish_odom(odom_x, odom_y, odom_theta,
                              vel_linear, vel_angular);
            uros_publish_balance_state(
                imu_filter_get_pitch(&imu_filt), fault_flags);
#endif
        }

        /* ── microROS spin (non-blocking) ── */
#ifdef UROS_ENABLED
        uros_spin_once(1);
#endif

        /* Yield to prevent tight spin — WFI waits for next interrupt */
        __WFI();
    }
}

/* ── System Clock: 180 MHz from 8 MHz HSE ──────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 360;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;   /* SYSCLK = 180 MHz */
    osc.PLL.PLLQ       = 8;
    HAL_RCC_OscConfig(&osc);

    /* Enable Over-Drive for 180 MHz */
    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                          RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK  = 180 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;      /* APB1  = 45 MHz, timers = 90 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;      /* APB2  = 90 MHz, timers = 180 MHz */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);
}

/* ── GPIO: LEDs, motor direction, encoder inputs ────────────── */
static void GPIO_Init(void)
{
    /* Enable all GPIO clocks we use */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* LEDs: PG13 green, PG14 red */
    gpio.Pin   = LED_GREEN_PIN | LED_RED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_PORT, &gpio);

    /* Motor 1 direction: PA5, PA7 */
    gpio.Pin = MOTOR1_DIR_A_PIN | MOTOR1_DIR_B_PIN;
    HAL_GPIO_Init(MOTOR1_DIR_A_PORT, &gpio);

    /* Motor 2 direction: PD4, PD5 */
    gpio.Pin = MOTOR2_DIR_A_PIN | MOTOR2_DIR_B_PIN;
    HAL_GPIO_Init(MOTOR2_DIR_A_PORT, &gpio);
}

/* ── I2C3: ICM-20948 (PA8=SCL, PC9=SDA) ────────────────────── */
static void I2C3_Init(void)
{
    __HAL_RCC_I2C3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C3;

    /* PA8 = SCL */
    gpio.Pin = ICM20948_SCL_PIN;
    HAL_GPIO_Init(ICM20948_SCL_PORT, &gpio);

    /* PC9 = SDA */
    gpio.Pin = ICM20948_SDA_PIN;
    HAL_GPIO_Init(ICM20948_SDA_PORT, &gpio);

    hi2c3.Instance             = I2C3;
    hi2c3.Init.ClockSpeed      = 400000;          /* 400 kHz fast mode */
    hi2c3.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c3.Init.OwnAddress1     = 0;
    hi2c3.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c3);
}

/* ── USART1: ESP32 bridge (PA9=TX, PA10=RX) ────────────────── */
static void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;

    gpio.Pin = ESP32_UART_TX_PIN;
    HAL_GPIO_Init(ESP32_UART_TX_PORT, &gpio);

    gpio.Pin = ESP32_UART_RX_PIN;
    HAL_GPIO_Init(ESP32_UART_RX_PORT, &gpio);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = UART_BAUD_RATE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* ── TIM6: 1 kHz timebase ──────────────────────────────────── */
static void TIM6_Init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* APB1 timer clock = 90 MHz. Prescaler=89 → 1 MHz tick, period=999 → 1 kHz */
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = 89;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = 999;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim6);

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* ── LED helper ─────────────────────────────────────────────── */
static void LED_Set(uint8_t green, uint8_t red)
{
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN,
                      green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN,
                      red ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ── Interrupt handlers ─────────────────────────────────────── */
void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        tick_1khz++;
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void NMI_Handler(void) {}
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void)  { while (1) {} }
void UsageFault_Handler(void){ while (1) {} }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* Expose handles for drivers */
I2C_HandleTypeDef  *icm20948_i2c_handle(void) { return &hi2c3; }
UART_HandleTypeDef *uros_uart_handle(void)    { return &huart1; }
