#pragma once

/*
 * icm20948.h
 * Driver for TDK InvenSense ICM-20948 — 9-axis IMU (accel + gyro + mag)
 * Interface: I2C1
 * Address: 0x68 (AD0=GND) or 0x69 (AD0=VDD)
 *
 * Yahboom lesson ref: ESP32 basic §10 (IMU read)
 */

#include <stdint.h>
#include <stdbool.h>

/* ── Register map (Bank 0) ───────────────────────────────────── */
#define ICM20948_REG_WHO_AM_I       0x00
#define ICM20948_WHO_AM_I_VAL       0xEA    /* expected device ID          */
#define ICM20948_REG_PWR_MGMT_1     0x06
#define ICM20948_REG_PWR_MGMT_2     0x07
#define ICM20948_REG_INT_PIN_CFG    0x0F
#define ICM20948_REG_INT_ENABLE_1   0x11
#define ICM20948_REG_ACCEL_XOUT_H   0x2D
#define ICM20948_REG_GYRO_XOUT_H    0x33
#define ICM20948_REG_TEMP_OUT_H     0x39
#define ICM20948_REG_BANK_SEL       0x7F

/* ── Register map (Bank 2) ───────────────────────────────────── */
#define ICM20948_B2_GYRO_CONFIG_1   0x01
#define ICM20948_B2_ACCEL_CONFIG    0x14
#define ICM20948_B2_ODR_GYRO        0x00
#define ICM20948_B2_ODR_ACCEL       0x10

/* ── Sensitivity scales ──────────────────────────────────────── */
#define ICM20948_GYRO_SENS_2000DPS  16.4f   /* LSB / (deg/s)               */
#define ICM20948_ACCEL_SENS_4G      8192.0f /* LSB / g                     */

/* ── Raw sensor data ─────────────────────────────────────────── */
typedef struct {
    float accel_x;   /* m/s^2 */
    float accel_y;
    float accel_z;
    float gyro_x;    /* deg/s */
    float gyro_y;
    float gyro_z;
    float mag_x;     /* uT    */
    float mag_y;
    float mag_z;
    float temp_c;    /* °C    */
} icm20948_data_t;

/* ── Calibration offsets ─────────────────────────────────────── */
typedef struct {
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float accel_bias_x;
    float accel_bias_y;
    float accel_bias_z;
} icm20948_calib_t;

/* ── API ─────────────────────────────────────────────────────── */

/**
 * @brief  Initialise ICM-20948: wake, configure ODR/FSR, verify WHO_AM_I.
 * @return true on success, false if device not found or I2C error.
 */
bool icm20948_init(void);

/**
 * @brief  Read all axes (accel + gyro + mag) into data struct.
 *         Applies calibration offsets if set.
 * @param  data  Output struct, populated on success.
 * @return true on success.
 */
bool icm20948_read(icm20948_data_t *data);

/**
 * @brief  Collect N samples at rest to compute and store bias offsets.
 *         Robot must be stationary and level during calibration.
 * @param  n_samples  Number of samples to average (recommend >= 1000).
 */
void icm20948_calibrate(uint16_t n_samples);

/**
 * @brief  Load pre-computed calibration offsets (from flash / NVS).
 */
void icm20948_set_calibration(const icm20948_calib_t *calib);

/**
 * @brief  Retrieve current calibration values.
 */
void icm20948_get_calibration(icm20948_calib_t *calib);

/**
 * @brief  Run device self-test. Returns true if all axes pass.
 */
bool icm20948_self_test(void);
