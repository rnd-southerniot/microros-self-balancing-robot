/*
 * uros_node.c
 * microROS node: publishers for /imu/data, /odom, /balance_state
 * Subscriber for /cmd_vel
 */

#ifdef UROS_ENABLED

#include "uros_node.h"
#include "robot_params.h"
#include "stm32f4xx_hal.h"

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <sensor_msgs/msg/imu.h>
#include <nav_msgs/msg/odometry.h>
#include <geometry_msgs/msg/twist.h>
#include <std_msgs/msg/float32.h>

#include <string.h>
#include <math.h>

/* ── Static allocations ─────────────────────────────────────── */
static rcl_allocator_t        allocator;
static rclc_support_t         support;
static rcl_node_t             node;

static rcl_publisher_t        pub_imu;
static rcl_publisher_t        pub_odom;
static rcl_publisher_t        pub_balance;

static rcl_subscription_t     sub_cmd_vel;
static rclc_executor_t        executor;

static sensor_msgs__msg__Imu           msg_imu;
static nav_msgs__msg__Odometry         msg_odom;
static std_msgs__msg__Float32          msg_balance;
static geometry_msgs__msg__Twist       msg_cmd_vel;

/* cmd_vel state */
static float    cv_linear_x  = 0.0f;
static float    cv_angular_z = 0.0f;
static uint32_t cv_stamp     = 0;

/* ── cmd_vel callback ───────────────────────────────────────── */
static void cmd_vel_callback(const void *msgin)
{
    const geometry_msgs__msg__Twist *msg = (const geometry_msgs__msg__Twist *)msgin;
    cv_linear_x  = (float)msg->linear.x;
    cv_angular_z = (float)msg->angular.z;
    cv_stamp     = HAL_GetTick();
}

/* ── Public API ─────────────────────────────────────────────── */

bool uros_node_init(void)
{
    allocator = rcl_get_default_allocator();

    /* Initialise support (connects to agent) */
    rcl_ret_t ret = rclc_support_init(&support, 0, NULL, &allocator);
    if (ret != RCL_RET_OK) return false;

    /* Create node */
    ret = rclc_node_init_default(&node, "stm32_balance", "", &support);
    if (ret != RCL_RET_OK) return false;

    /* Publishers */
    ret = rclc_publisher_init_best_effort(
        &pub_imu, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
        "/imu/data");
    if (ret != RCL_RET_OK) return false;

    ret = rclc_publisher_init_best_effort(
        &pub_odom, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
        "/odom");
    if (ret != RCL_RET_OK) return false;

    ret = rclc_publisher_init_best_effort(
        &pub_balance, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
        "/balance_state/pitch_deg");
    if (ret != RCL_RET_OK) return false;

    /* Subscriber: /cmd_vel */
    ret = rclc_subscription_init_default(
        &sub_cmd_vel, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "/cmd_vel");
    if (ret != RCL_RET_OK) return false;

    /* Executor with 1 subscription */
    ret = rclc_executor_init(&executor, &support.context, 1, &allocator);
    if (ret != RCL_RET_OK) return false;

    ret = rclc_executor_add_subscription(&executor, &sub_cmd_vel,
                                          &msg_cmd_vel, &cmd_vel_callback,
                                          ON_NEW_DATA);
    if (ret != RCL_RET_OK) return false;

    /* Initialise message frame IDs */
    /* IMU */
    memset(&msg_imu, 0, sizeof(msg_imu));
    msg_imu.header.frame_id.data     = "imu_link";
    msg_imu.header.frame_id.size     = 8;
    msg_imu.header.frame_id.capacity = 9;

    /* Odom */
    memset(&msg_odom, 0, sizeof(msg_odom));
    msg_odom.header.frame_id.data          = "odom";
    msg_odom.header.frame_id.size          = 4;
    msg_odom.header.frame_id.capacity      = 5;
    msg_odom.child_frame_id.data           = "base_link";
    msg_odom.child_frame_id.size           = 9;
    msg_odom.child_frame_id.capacity       = 10;

    return true;
}

void uros_publish_imu(const icm20948_data_t *data)
{
    /* Timestamp */
    int64_t now_ns = (int64_t)HAL_GetTick() * 1000000LL;
    msg_imu.header.stamp.sec     = (int32_t)(now_ns / 1000000000LL);
    msg_imu.header.stamp.nanosec = (uint32_t)(now_ns % 1000000000LL);

    /* Linear acceleration (m/s²) */
    msg_imu.linear_acceleration.x = data->accel_x;
    msg_imu.linear_acceleration.y = data->accel_y;
    msg_imu.linear_acceleration.z = data->accel_z;

    /* Angular velocity (rad/s — convert from deg/s) */
    const float deg2rad = 0.017453292f;
    msg_imu.angular_velocity.x = data->gyro_x * deg2rad;
    msg_imu.angular_velocity.y = data->gyro_y * deg2rad;
    msg_imu.angular_velocity.z = data->gyro_z * deg2rad;

    /* Orientation not estimated (set covariance[0] = -1) */
    msg_imu.orientation_covariance[0] = -1.0;

    rcl_publish(&pub_imu, &msg_imu, NULL);
}

void uros_publish_odom(float x, float y, float theta, float vx, float vth)
{
    int64_t now_ns = (int64_t)HAL_GetTick() * 1000000LL;
    msg_odom.header.stamp.sec     = (int32_t)(now_ns / 1000000000LL);
    msg_odom.header.stamp.nanosec = (uint32_t)(now_ns % 1000000000LL);

    /* Pose */
    msg_odom.pose.pose.position.x = x;
    msg_odom.pose.pose.position.y = y;
    msg_odom.pose.pose.position.z = 0.0;

    /* Quaternion from yaw */
    float half_theta = theta * 0.5f;
    msg_odom.pose.pose.orientation.x = 0.0;
    msg_odom.pose.pose.orientation.y = 0.0;
    msg_odom.pose.pose.orientation.z = sinf(half_theta);
    msg_odom.pose.pose.orientation.w = cosf(half_theta);

    /* Twist */
    msg_odom.twist.twist.linear.x  = vx;
    msg_odom.twist.twist.angular.z = vth;

    rcl_publish(&pub_odom, &msg_odom, NULL);
}

void uros_publish_balance_state(float pitch_deg, uint8_t fault_flags)
{
    (void)fault_flags;  /* TODO: use custom message type */
    msg_balance.data = pitch_deg;
    rcl_publish(&pub_balance, &msg_balance, NULL);
}

void uros_spin_once(uint32_t timeout_ms)
{
    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(timeout_ms));
}

void uros_get_cmd_vel(float *linear_x, float *angular_z)
{
    *linear_x  = cv_linear_x;
    *angular_z = cv_angular_z;
}

uint32_t uros_get_cmd_vel_stamp(void)
{
    return cv_stamp;
}

#endif /* UROS_ENABLED */
