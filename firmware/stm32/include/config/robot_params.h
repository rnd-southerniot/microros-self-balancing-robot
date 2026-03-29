#pragma once

/*
 * robot_params.h
 * Single source of truth for all physical constants, control loop
 * rates, PID gains, and safety thresholds.
 *
 * Hardware: STM32F429I-DISC1
 * IMU primary:   ICM-20948 (I2C3: PA8=SCL, PC9=SDA, 9-axis)
 * IMU secondary: L3GD20    (SPI1, onboard Discovery — cross-check)
 * Motors: 520 encoder motors, x2
 *
 * Gains are STARTING POINTS. Tune via SetPidGains.srv at runtime.
 * Commit tuned values back here after validation.
 */

/* ── Physical constants ──────────────────────────────────────── */
#define WHEEL_RADIUS_M          0.0335f   /* metres — measure your wheels */
#define WHEEL_BASE_M            0.160f    /* metres — measure chassis      */
#define ENCODER_CPR             1320      /* counts/rev: 520 motor + gearbox */
#define GEAR_RATIO              30.0f     /* internal gear ratio            */

/* ── IMU: ICM-20948 primary (I2C3: PA8/PC9) ─────────────────── */
#define ICM20948_I2C_ADDR       0x68      /* AD0 pin = GND → 0x68          */
#define ICM20948_GYRO_FSR       2000      /* ±2000 dps full-scale           */
#define ICM20948_ACCEL_FSR      4         /* ±4g full-scale                 */
#define ICM20948_SAMPLE_RATE_HZ 500       /* ODR: 500 Hz                   */

/* ── IMU: L3GD20 secondary (SPI1, onboard Discovery) ────────── */
#define L3GD20_GYRO_FSR         2000      /* ±2000 dps full-scale           */
#define L3GD20_DIVERGE_THRESHOLD 5.0f     /* deg/s — fault if >5 vs primary */

/* ── Complementary filter ────────────────────────────────────── */
#define IMU_FILTER_ALPHA        0.98f     /* gyro weight: 0.98 = mostly gyro */
#define IMU_ZERO_ANGLE_DEG      0.0f      /* equilibrium angle — calibrate   */

/* ── Control loop rates (Hz) ─────────────────────────────────── */
#define IMU_SAMPLE_HZ           500
#define BALANCE_CTRL_HZ         200
#define ODOM_PUB_HZ             50
#define UROS_SPIN_HZ            100
#define SAFETY_CHECK_HZ         200

/* ── PID: angle (inner) loop ─────────────────────────────────── */
/* Tune first. Zero velocity setpoint. Robot on test stand.       */
#define PID_ANGLE_KP            25.0f
#define PID_ANGLE_KI            0.0f
#define PID_ANGLE_KD            0.8f
#define PID_ANGLE_IMAX          50.0f     /* anti-windup clamp              */

/* ── PID: velocity (outer) loop ──────────────────────────────── */
/* Tune second. Wraps angle loop. Robot free-standing.            */
#define PID_VEL_KP              2.5f
#define PID_VEL_KI              0.3f
#define PID_VEL_KD              0.0f
#define PID_VEL_IMAX            10.0f

/* ── PID: steering (yaw) loop ────────────────────────────────── */
#define PID_STEER_KP            3.0f
#define PID_STEER_KI            0.0f
#define PID_STEER_KD            0.1f
#define PID_STEER_IMAX          5.0f

/* ── PID: speed (per-wheel) loop ─────────────────────────────── */
#define PID_SPEED_KP            2.0f
#define PID_SPEED_KI            0.5f
#define PID_SPEED_KD            0.0f
#define PID_SPEED_IMAX          20.0f

/* ── Safety thresholds ───────────────────────────────────────── */
#define MAX_TILT_DEG            45.0f     /* e-stop if exceeded             */
#define MAX_MOTOR_DUTY          0.90f     /* 90% PWM ceiling                */
#define CMD_VEL_TIMEOUT_MS      500       /* zero velocity if no packet     */
#define BATTERY_LOW_MV          6800      /* ~3.4V/cell for 2S LiPo         */

/* ── UART protocol (STM32 ↔ ESP32) ──────────────────────────── */
#define UART_BAUD_RATE          921600
#define UART_FRAME_START        0xAA
#define UART_FRAME_END          0x55
#define UART_MAX_PAYLOAD_BYTES  64
