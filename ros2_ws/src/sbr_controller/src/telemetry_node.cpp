/**
 * telemetry_node.cpp
 *
 * Aggregates system health from multiple sources and publishes
 * a unified /robot_status topic at 1 Hz for monitoring and alerting.
 *
 * Subscribes:
 *   /balance_state       (sbr_interfaces/BalanceState)
 *   /safety/estopped     (std_msgs/Bool)
 *   /imu/data            (sensor_msgs/Imu)  — heartbeat check
 *
 * Publishes:
 *   /robot_status        (sbr_interfaces/RobotStatus @ 1 Hz)
 *
 * Yahboom lesson ref: ROS2 basic §7, §11 (topic + parameter)
 */

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sbr_interfaces/msg/balance_state.hpp>
#include <sbr_interfaces/msg/robot_status.hpp>
#include <chrono>
#include <string>

using namespace std::chrono_literals;
using BalanceState = sbr_interfaces::msg::BalanceState;
using RobotStatus  = sbr_interfaces::msg::RobotStatus;

class TelemetryNode : public rclcpp::Node
{
public:
    TelemetryNode()
    : Node("telemetry_node"),
      start_time_(this->now())
    {
        this->declare_parameter("publish_rate_hz", 1.0);
        this->declare_parameter("firmware_version", std::string("0.1.0"));

        firmware_version_ = this->get_parameter("firmware_version")
                                .as_string();
        double rate_hz    = this->get_parameter("publish_rate_hz").as_double();

        /* ── Subscribers ─────────────────────────────────────────── */
        balance_sub_ = this->create_subscription<BalanceState>(
            "/balance_state", 10,
            [this](const BalanceState::SharedPtr msg) {
                latest_balance_ = *msg;
                has_balance_ = true;
            });

        estop_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/safety/estopped", 10,
            [this](const std_msgs::msg::Bool::SharedPtr msg) {
                is_estopped_ = msg->data;
            });

        imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", 100,
            [this](const sensor_msgs::msg::Imu::SharedPtr) {
                last_imu_stamp_ = this->now();
                imu_alive_ = true;
            });

        /* ── Publisher ───────────────────────────────────────────── */
        status_pub_ = this->create_publisher<RobotStatus>(
            "/robot_status", 10);

        /* ── Publish timer ───────────────────────────────────────── */
        auto period = std::chrono::duration<double>(1.0 / rate_hz);
        timer_ = this->create_wall_timer(
            std::chrono::duration_cast<std::chrono::nanoseconds>(period),
            [this]() { this->publish_status(); });

        RCLCPP_INFO(this->get_logger(),
            "telemetry_node started @ %.1f Hz — firmware v%s",
            rate_hz, firmware_version_.c_str());
    }

private:
    void publish_status()
    {
        RobotStatus msg;
        msg.header.stamp    = this->now();
        msg.header.frame_id = "base_link";

        /* Uptime */
        auto elapsed = this->now() - start_time_;
        msg.uptime_s = static_cast<uint32_t>(elapsed.seconds());

        /* Balance state passthrough */
        if (has_balance_) {
            msg.fault_flags   = latest_balance_.fault_flags;
            msg.is_balancing  = (latest_balance_.fault_flags == 0);
        } else {
            msg.fault_flags   = 0xFF;   /* unknown — no balance data yet */
            msg.is_balancing  = false;
        }

        /* E-stop */
        msg.is_estopped = is_estopped_;

        /* IMU heartbeat proxy for WiFi RSSI placeholder */
        /* TODO: read actual WiFi RSSI from ESP32 via custom topic */
        msg.wifi_rssi_dbm = -70.0f;   /* placeholder */

        /* Battery — TODO: subscribe to /battery topic from STM32 */
        msg.battery_mv = 0.0f;        /* placeholder */

        /* CPU temp — TODO: read /sys/class/thermal on Pi 5 */
        msg.cpu_temp_c = 0.0f;        /* placeholder */

        msg.firmware_version = firmware_version_;

        status_pub_->publish(msg);

        /* Log summary at INFO level */
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
            "uptime=%us  balancing=%s  estopped=%s  faults=0x%02X",
            msg.uptime_s,
            msg.is_balancing ? "YES" : "NO",
            msg.is_estopped  ? "YES" : "NO",
            msg.fault_flags);
    }

    /* ── Members ──────────────────────────────────────────────── */
    rclcpp::Subscription<BalanceState>::SharedPtr       balance_sub_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr estop_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Publisher<RobotStatus>::SharedPtr           status_pub_;
    rclcpp::TimerBase::SharedPtr                        timer_;

    BalanceState  latest_balance_{};
    bool          has_balance_{false};
    bool          is_estopped_{false};
    bool          imu_alive_{false};
    rclcpp::Time  last_imu_stamp_;
    rclcpp::Time  start_time_;
    std::string   firmware_version_{"0.1.0"};
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<TelemetryNode>());
    rclcpp::shutdown();
    return 0;
}
