#pragma once

/*
 * uros_node.h
 * microROS node with publishers for IMU, odometry, and balance state.
 * Subscriber for cmd_vel.
 */

#ifdef UROS_ENABLED

#include <stdbool.h>
#include "icm20948.h"

/**
 * @brief  Initialise microROS node, publishers, subscribers.
 *         Call after uros_transport_init().
 * @return true on success.
 */
bool uros_node_init(void);

/**
 * @brief  Publish /imu/data message.
 * @param  data  Fresh IMU reading.
 */
void uros_publish_imu(const icm20948_data_t *data);

/**
 * @brief  Publish /odom message.
 * @param  x, y, theta  Pose in odom frame.
 * @param  vx, vth      Linear and angular velocity.
 */
void uros_publish_odom(float x, float y, float theta, float vx, float vth);

/**
 * @brief  Publish /balance_state message.
 * @param  pitch_deg    Current pitch angle.
 * @param  fault_flags  Bitfield of active faults.
 */
void uros_publish_balance_state(float pitch_deg, uint8_t fault_flags);

/**
 * @brief  Spin the executor (process callbacks). Call from main loop.
 *         Non-blocking with timeout_ms.
 */
void uros_spin_once(uint32_t timeout_ms);

/**
 * @brief  Get latest cmd_vel values (from /cmd_vel subscriber).
 */
void uros_get_cmd_vel(float *linear_x, float *angular_z);

/**
 * @brief  Get timestamp of last cmd_vel message (HAL tick).
 */
uint32_t uros_get_cmd_vel_stamp(void);

#endif /* UROS_ENABLED */
