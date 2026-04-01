/*
 * imu_filter.c — Complementary filter for pitch/roll estimation.
 * See imu_filter.h for theory and API docs.
 */

#include "imu_filter.h"
#include "robot_params.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void imu_filter_init(imu_filter_t *filt, float alpha, float dt)
{
    filt->pitch       = 0.0f;
    filt->roll        = 0.0f;
    filt->pitch_rate  = 0.0f;
    filt->roll_rate   = 0.0f;
    filt->alpha       = alpha;
    filt->dt          = dt;
    filt->initialised = true;
}

imu_fault_t imu_filter_update(imu_filter_t *filt, const icm20948_data_t *data)
{
    /* Accelerometer-derived angles */
    float accel_pitch = atan2f(data->accel_x,
                               sqrtf(data->accel_y * data->accel_y +
                                     data->accel_z * data->accel_z))
                        * (180.0f / M_PI);

    float accel_roll = atan2f(data->accel_y, data->accel_z)
                       * (180.0f / M_PI);

    /* Store angular rates */
    filt->pitch_rate = data->gyro_y;
    filt->roll_rate  = data->gyro_x;

    /* Complementary filter fusion */
    filt->pitch = filt->alpha * (filt->pitch + data->gyro_y * filt->dt)
                + (1.0f - filt->alpha) * accel_pitch;

    filt->roll  = filt->alpha * (filt->roll + data->gyro_x * filt->dt)
                + (1.0f - filt->alpha) * accel_roll;

    return IMU_OK;
}

bool imu_filter_cross_check(float primary_gyro_z, float secondary_gyro_z)
{
    float diff = fabsf(primary_gyro_z - secondary_gyro_z);
    return diff < L3GD20_DIVERGE_THRESHOLD;
}

void imu_filter_reset(imu_filter_t *filt)
{
    filt->pitch       = 0.0f;
    filt->roll        = 0.0f;
    filt->pitch_rate  = 0.0f;
    filt->roll_rate   = 0.0f;
    filt->initialised = false;
}

float imu_filter_get_pitch(const imu_filter_t *filt)
{
    return filt->pitch;
}
