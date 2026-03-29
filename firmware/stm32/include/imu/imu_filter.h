#pragma once

/*
 * imu_filter.h
 * Complementary filter fusing ICM-20948 gyro + accelerometer
 * to produce stable pitch and roll estimates.
 *
 * Primary source: ICM-20948 (I2C3: PA8/PC9)
 * Secondary/cross-check: L3GD20 (SPI1, onboard Discovery)
 *
 * Theory:
 *   angle = alpha * (angle + gyro_rate * dt)
 *         + (1 - alpha) * accel_angle
 *
 *   alpha = IMU_FILTER_ALPHA (default 0.98)
 *   High alpha → trust gyro (low noise, drifts over time)
 *   Low alpha  → trust accel (noisy, but no drift)
 */

#include <stdint.h>
#include <stdbool.h>
#include "icm20948.h"

/* ── Filter state ────────────────────────────────────────────── */
typedef struct {
    float pitch;          /* deg — positive = nose up               */
    float roll;           /* deg — positive = right side up         */
    float pitch_rate;     /* deg/s — from gyro                      */
    float roll_rate;
    float alpha;          /* filter coefficient [0..1]              */
    float dt;             /* sample period seconds (1/IMU_SAMPLE_HZ)*/
    bool  initialised;
} imu_filter_t;

/* ── Fault flags ─────────────────────────────────────────────── */
typedef enum {
    IMU_OK             = 0x00,
    IMU_FAULT_PRIMARY  = 0x01,  /* ICM-20948 read failure            */
    IMU_FAULT_SECONDARY= 0x02,  /* L3GD20 read failure               */
    IMU_FAULT_DIVERGED = 0x04,  /* primary/secondary gyro mismatch   */
} imu_fault_t;

/* ── API ─────────────────────────────────────────────────────── */

/**
 * @brief  Initialise filter with alpha and sample period.
 * @param  filt   Filter state struct to initialise.
 * @param  alpha  Gyro weight [0.9 .. 0.99]. Typical: 0.98.
 * @param  dt     Sample period in seconds. E.g. 1.0f/500.
 */
void imu_filter_init(imu_filter_t *filt, float alpha, float dt);

/**
 * @brief  Update filter with latest sensor data.
 *         Call at IMU_SAMPLE_HZ (500 Hz).
 * @param  filt  Filter state (updated in-place).
 * @param  data  Fresh ICM-20948 reading.
 * @return Fault flags (IMU_OK = 0 means all good).
 */
imu_fault_t imu_filter_update(imu_filter_t *filt,
                               const icm20948_data_t *data);

/**
 * @brief  Cross-check primary gyro Z against L3GD20 gyro Z.
 *         Sets IMU_FAULT_DIVERGED if |diff| > L3GD20_DIVERGE_THRESHOLD.
 * @param  primary_gyro_z   ICM-20948 gyro Z (deg/s).
 * @param  secondary_gyro_z L3GD20 gyro Z (deg/s).
 * @return true if sensors agree (no fault).
 */
bool imu_filter_cross_check(float primary_gyro_z, float secondary_gyro_z);

/**
 * @brief  Reset filter state (e.g. after fault recovery).
 */
void imu_filter_reset(imu_filter_t *filt);

/**
 * @brief  Get current pitch angle (degrees). Positive = nose up.
 */
float imu_filter_get_pitch(const imu_filter_t *filt);
