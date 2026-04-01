/*
 * icm20948.c — ICM-20948 9-axis IMU driver over I2C3.
 * PA8=SCL, PC9=SDA, address 0x68 (AD0=GND).
 */

#ifndef NATIVE_TEST

#include "icm20948.h"
#include "robot_params.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <math.h>

/* I2C handle defined in main.c */
extern I2C_HandleTypeDef *icm20948_i2c_handle(void);

#define ADDR_W  (ICM20948_I2C_ADDR << 1)
#define I2C_TMO 50  /* ms */

static icm20948_calib_t calib = {0};

/* ── Low-level I2C helpers ──────────────────────────────────── */

static bool reg_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(icm20948_i2c_handle(), ADDR_W, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, I2C_TMO) == HAL_OK;
}

static bool reg_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(icm20948_i2c_handle(), ADDR_W, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, I2C_TMO) == HAL_OK;
}

static bool set_bank(uint8_t bank)
{
    return reg_write(ICM20948_REG_BANK_SEL, (bank & 0x03) << 4);
}

/* ── Public API ─────────────────────────────────────────────── */

bool icm20948_init(void)
{
    HAL_Delay(100);  /* power-on delay */

    /* Select bank 0 */
    if (!set_bank(0)) return false;

    /* Verify WHO_AM_I */
    uint8_t id = 0;
    if (!reg_read(ICM20948_REG_WHO_AM_I, &id, 1)) return false;
    if (id != ICM20948_WHO_AM_I_VAL) return false;

    /* Reset device */
    reg_write(ICM20948_REG_PWR_MGMT_1, 0x80);
    HAL_Delay(100);

    /* Wake up, auto-select clock */
    reg_write(ICM20948_REG_PWR_MGMT_1, 0x01);
    HAL_Delay(50);

    /* Enable all accel + gyro axes */
    reg_write(ICM20948_REG_PWR_MGMT_2, 0x00);

    /* Bank 2: configure gyro — ±2000 dps, DLPF enabled */
    set_bank(2);
    reg_write(ICM20948_B2_GYRO_CONFIG_1, 0x06 | 0x01);  /* FS=±2000, DLPF_CFG=0, EN=1 */

    /* Bank 2: configure accel — ±4g, DLPF enabled */
    reg_write(ICM20948_B2_ACCEL_CONFIG, 0x04 | 0x01);    /* FS=±4g, DLPF_CFG=0, EN=1 */

    /* Back to bank 0 */
    set_bank(0);

    return true;
}

bool icm20948_read(icm20948_data_t *data)
{
    uint8_t buf[12];

    /* Read accel (6 bytes from ACCEL_XOUT_H) */
    if (!reg_read(ICM20948_REG_ACCEL_XOUT_H, buf, 6)) return false;

    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);

    /* Read gyro (6 bytes from GYRO_XOUT_H) */
    if (!reg_read(ICM20948_REG_GYRO_XOUT_H, buf, 6)) return false;

    int16_t gx = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t gy = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t gz = (int16_t)((buf[4] << 8) | buf[5]);

    /* Read temperature (2 bytes) */
    if (!reg_read(ICM20948_REG_TEMP_OUT_H, buf, 2)) return false;
    int16_t temp_raw = (int16_t)((buf[0] << 8) | buf[1]);

    /* Convert to physical units */
    const float accel_scale = 9.80665f / ICM20948_ACCEL_SENS_4G;
    const float gyro_scale  = 1.0f / ICM20948_GYRO_SENS_2000DPS;

    data->accel_x = (float)ax * accel_scale - calib.accel_bias_x;
    data->accel_y = (float)ay * accel_scale - calib.accel_bias_y;
    data->accel_z = (float)az * accel_scale - calib.accel_bias_z;
    data->gyro_x  = (float)gx * gyro_scale  - calib.gyro_bias_x;
    data->gyro_y  = (float)gy * gyro_scale  - calib.gyro_bias_y;
    data->gyro_z  = (float)gz * gyro_scale  - calib.gyro_bias_z;

    data->temp_c = (float)temp_raw / 333.87f + 21.0f;

    /* Magnetometer not read yet — zeroed */
    data->mag_x = data->mag_y = data->mag_z = 0.0f;

    return true;
}

void icm20948_calibrate(uint16_t n_samples)
{
    float gx_sum = 0, gy_sum = 0, gz_sum = 0;
    float ax_sum = 0, ay_sum = 0, az_sum = 0;

    /* Temporarily clear calibration so raw values are used */
    icm20948_calib_t saved = calib;
    memset(&calib, 0, sizeof(calib));

    icm20948_data_t d;
    uint16_t good = 0;
    for (uint16_t i = 0; i < n_samples; i++) {
        HAL_Delay(2);  /* ~500 Hz */
        if (icm20948_read(&d)) {
            gx_sum += d.gyro_x;
            gy_sum += d.gyro_y;
            gz_sum += d.gyro_z;
            ax_sum += d.accel_x;
            ay_sum += d.accel_y;
            az_sum += d.accel_z;
            good++;
        }
    }

    if (good > 0) {
        calib.gyro_bias_x  = gx_sum / good;
        calib.gyro_bias_y  = gy_sum / good;
        calib.gyro_bias_z  = gz_sum / good;
        calib.accel_bias_x = ax_sum / good;
        calib.accel_bias_y = ay_sum / good;
        /* Z axis: subtract gravity so bias = reading - 9.81 */
        calib.accel_bias_z = (az_sum / good) - 9.80665f;
    } else {
        calib = saved;  /* restore if failed */
    }
}

void icm20948_set_calibration(const icm20948_calib_t *c) { calib = *c; }
void icm20948_get_calibration(icm20948_calib_t *c)       { *c = calib; }

bool icm20948_self_test(void)
{
    /* Simplified: verify WHO_AM_I and valid data */
    set_bank(0);
    uint8_t id = 0;
    if (!reg_read(ICM20948_REG_WHO_AM_I, &id, 1)) return false;
    if (id != ICM20948_WHO_AM_I_VAL) return false;

    icm20948_data_t d;
    if (!icm20948_read(&d)) return false;

    /* Sanity: gravity should be ~9.81 m/s² on one axis */
    float mag = sqrtf(d.accel_x*d.accel_x + d.accel_y*d.accel_y + d.accel_z*d.accel_z);
    return (mag > 7.0f && mag < 13.0f);
}

#endif /* !NATIVE_TEST */
