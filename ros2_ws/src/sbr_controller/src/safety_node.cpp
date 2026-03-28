/**
 * safety_node.cpp
 *
 * E-stop watchdog for the MicroROS self-balancing robot.
 * Monitors two conditions and publishes zero cmd_vel if either triggers:
 *
 *   1. Tilt fault  — pitch exceeds MAX_TILT_DEG (robot has fallen)
 *   2. Heartbeat   — no /imu/data received within CMD_VEL_TIMEOUT_MS
 *
 * When either condition fires:
 *   - Publishes geometry_msgs/Twist{0} directly to /cmd_vel
 *   - Sets is_estopped flag in /robot_status
 *   - Logs a single warning (rate-limited to avoid log spam)
 *
 * Recovery: fault clears automatically when tilt returns to safe range
 * AND IMU heartbeat resumes. No manual reset required.
 *
 * Yahboom lesson ref: ROS2 basic §7 (topic communication)
 */

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <std_msgs/msg/bool.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

class SafetyNode : public rclcpp::Node
{
public:
    SafetyNode()
    : Node("safety_node"),
      is_estopped_(false),
      last_imu_time_(this->now())
    {
        /* ── Parameters ─────────────────────────────────────────── */
        this->declare_parameter("max_tilt_deg",       45.0);
        this->declare_parameter("heartbeat_timeout_ms", 500);
        this->declare_parameter("check_rate_hz",       200.0);

        max_tilt_deg_       = this->get_parameter("max_tilt_deg").as_double();
        heartbeat_timeout_  = std::chrono::milliseconds(
            this->get_parameter("heartbeat_timeout_ms").as_int());
        double rate_hz      = this->get_parameter("check_rate_hz").as_double();

        /* ── Subscribers ────────────────────────────────────────── */
        imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", 10,
            [this](const sensor_msgs::msg::Imu::SharedPtr msg) {
                this->imu_callback(msg);
            });

        /* ── Publishers ─────────────────────────────────────────── */
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(
            "/cmd_vel", 1);

        estop_pub_ = this->create_publisher<std_msgs::msg::Bool>(
            "/safety/estopped", 1);

        /* ── Safety check timer ─────────────────────────────────── */
        auto period = std::chrono::duration<double>(1.0 / rate_hz);
        timer_ = this->create_wall_timer(
            std::chrono::duration_cast<std::chrono::nanoseconds>(period),
            [this]() { this->safety_check(); });

        RCLCPP_INFO(this->get_logger(),
            "safety_node started — max_tilt=%.1f° timeout=%ldms",
            max_tilt_deg_,
            static_cast<long>(heartbeat_timeout_.count()));
    }

private:
    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
    {
        last_imu_time_ = this->now();

        /* Extract pitch from quaternion */
        tf2::Quaternion q(
            msg->orientation.x,
            msg->orientation.y,
            msg->orientation.z,
            msg->orientation.w);
        tf2::Matrix3x3 m(q);
        double roll, pitch, yaw;
        m.getRPY(roll, pitch, yaw);

        current_pitch_deg_ = pitch * 180.0 / M_PI;
    }

    void safety_check()
    {
        bool tilt_fault     = std::abs(current_pitch_deg_) > max_tilt_deg_;
        auto now            = this->now();
        auto elapsed        = now - last_imu_time_;
        bool heartbeat_fault = elapsed > rclcpp::Duration(heartbeat_timeout_);

        bool should_estop = tilt_fault || heartbeat_fault;

        if (should_estop && !is_estopped_) {
            is_estopped_ = true;
            if (tilt_fault) {
                RCLCPP_WARN(this->get_logger(),
                    "E-STOP: tilt fault — pitch=%.1f° > %.1f°",
                    current_pitch_deg_, max_tilt_deg_);
            }
            if (heartbeat_fault) {
                RCLCPP_WARN(this->get_logger(),
                    "E-STOP: IMU heartbeat lost (%.0f ms)",
                    elapsed.seconds() * 1000.0);
            }
        } else if (!should_estop && is_estopped_) {
            is_estopped_ = false;
            RCLCPP_INFO(this->get_logger(), "E-stop cleared — resuming");
        }

        /* Always publish zero if e-stopped */
        if (is_estopped_) {
            geometry_msgs::msg::Twist zero{};
            cmd_vel_pub_->publish(zero);
        }

        /* Publish e-stop status flag */
        std_msgs::msg::Bool estop_msg;
        estop_msg.data = is_estopped_;
        estop_pub_->publish(estop_msg);
    }

    /* ── Members ──────────────────────────────────────────────── */
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr estop_pub_;
    rclcpp::TimerBase::SharedPtr timer_;

    double max_tilt_deg_{45.0};
    std::chrono::milliseconds heartbeat_timeout_{500};
    double current_pitch_deg_{0.0};
    bool is_estopped_{false};
    rclcpp::Time last_imu_time_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SafetyNode>());
    rclcpp::shutdown();
    return 0;
}
