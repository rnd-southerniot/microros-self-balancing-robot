# Yahboom Lesson Pack — Progress Tracker

Reference: https://www.yahboom.net/study/SBR-microROS

---

## Track 1: ESP32 Basic Course

| # | Lesson | Status | Notes | File |
|---|--------|--------|-------|------|
| 1 | Turn on LED light | 🔲 | GPIO OUTPUT on STM32F429 | `src/main.c` |
| 2 | Button function | 🔲 | PA0 user button | `src/main.c` |
| 3 | Drive the buzzer | 🔲 | PWM or GPIO toggle | — |
| 4 | Serial communication | 🔲 | USART1 @ 921600 to ESP32 | `src/uros/` |
| 5 | Battery voltage detection | 🔲 | ADC on STM32 | — |
| 6 | Drive PWM servo | 🔲 | TIM output compare | — |
| 7 | Drive motor | 🔲 | TIM3/TIM4 + H-bridge | `src/motor/motor_driver.c` |
| 8 | Read motor encoder data | 🔲 | Encoder mode | `src/motor/encoder.c` |
| 9 | PID controls car speed | 🔲 | Per-wheel speed loop | `src/pid/pid.c` |
| 10 | Read IMU data | 🔲 | ICM-20948 I2C + L3GD20 SPI | `src/imu/` |
| 11 | Read radar data | 🔲 | HC-SR04 TIM input capture | `src/sensors/` |
| 12 | Flash access data | 🔲 | STM32 flash / NVS | — |
| 13 | Partition table and memory | 🔲 | Memory layout study | — |
| 14 | Bluetooth communication | 🔲 | Optional — not primary path | — |
| 15 | WiFi networking | 🔲 | ESP32 WiFi + UDP | `firmware/esp32/src/` |
| 16 | Robot kinematics analysis | 🔲 | Two-wheel diff drive | `src/motor/odometry.c` |

---

## Track 2: microROS Control Board Environment

| # | Lesson | Status | Notes | File |
|---|--------|--------|-------|------|
| 1 | Intro to microROS control board | 🔲 | Architecture study | `docs/software/architecture.md` |
| 2 | Set up ESP32-IDF dev env | 🔲 | ESP-IDF v5.x install | `firmware/esp32/` |
| 3 | ESP32-IDF configuration tool | 🔲 | menuconfig | `firmware/esp32/platformio.ini` |
| 4 | Install ESP32-microros components | 🔲 | micro_ros_espidf_component | — |
| 5 | Install and start microROS agent | 🔲 | Docker on Pi 5 | `docker/docker-compose.yml` |
| 6 | Flash-tool firmware burn | 🔲 | `pio run -t upload` | `tools/flash/` |

---

## Track 3: microROS Basic Course

| # | Lesson | Status | Notes | File |
|---|--------|--------|-------|------|
| 1 | Publish topic | 🔲 | First `/imu/data` publish | `src/uros/publishers.c` |
| 2 | Subscribe topic | 🔲 | `/cmd_vel` subscribe | `src/uros/subscribers.c` |
| 3 | Multi-topic subscribe and publish | 🔲 | Full data pipeline | `src/uros/uros_node.c` |
| 4 | Subscribe buzzer topics | 🔲 | GPIO control from ROS2 | — |
| 5 | Subscribe PWM servo topics | 🔲 | — | — |
| 6 | Subscribe speed control topics | 🔲 | `/cmd_vel` → motor PWM | `src/uros/subscribers.c` |
| 7 | Release speed topic | 🔲 | Publish encoder velocity | `src/uros/publishers.c` |
| 8 | Release IMU data topic | 🔲 | `/imu/data` full msg | `src/uros/publishers.c` |
| 9 | Publish lidar data topics | 🔲 | Phase 5 | — |
| 10 | Customised transmission method | 🔲 | Binary frame protocol | `include/config/` |

---

## Track 4: ROS2 Basic Course (on Raspberry Pi 5)

| # | Lesson | Status | Notes | File |
|---|--------|--------|-------|------|
| 1 | Intro to ROS2 | 🔲 | Architecture overview | `docs/software/architecture.md` |
| 2 | ROS2 install Humble | 🔲 | Pi 5 Ubuntu 24.04 | `scripts/setup_pi5.sh` |
| 3 | ROS2 development environment | 🔲 | colcon workspace | `ros2_ws/` |
| 4 | ROS2 workspace | 🔲 | `colcon build` | `ros2_ws/` |
| 5 | ROS2 function package | 🔲 | `sbr_*` packages | `ros2_ws/src/` |
| 6 | ROS2 node | 🔲 | safety_node, telemetry_node | `ros2_ws/src/sbr_controller/` |
| 7 | ROS2 topic communication | 🔲 | pub/sub pattern | All nodes |
| 8 | ROS2 service communication | 🔲 | SetPidGains.srv | `ros2_ws/src/sbr_interfaces/` |
| 9 | ROS2 action communication | 🔲 | Nav2 actions (Phase 5) | — |
| 10 | ROS2 custom interface message | 🔲 | BalanceState, RobotStatus | `ros2_ws/src/sbr_interfaces/` |
| 11 | ROS2 parameter service | 🔲 | Node params | All nodes |
| 12 | ROS2 meta-function package | 🔲 | sbr_bringup | `ros2_ws/src/sbr_bringup/` |
| 13 | ROS2 distributed communication | 🔲 | Pi 5 ↔ Host VM | `ROS_DOMAIN_ID` |
| 14 | ROS2 DDS | 🔲 | QoS profiles | `ros2_ws/` |
| 15 | ROS2 time related API | 🔲 | Header stamps, clocks | All nodes |
| 16 | ROS2 common command tools | 🔲 | `ros2 topic / node / service` | `scripts/check_topics.py` |
| 17 | ROS2 rviz2 | 🔲 | TF, robot model | `ros2_ws/src/sbr_description/` |
| 18 | ROS2 rqt toolbox | 🔲 | rqt_plot for PID tuning | `docs/software/pid-tuning.md` |
| 19 | ROS2 Launch startup file | 🔲 | robot.launch.py | `ros2_ws/src/sbr_bringup/launch/` |
| 20 | ROS2 recording and playback | 🔲 | ros2 bag | `scripts/` |
| 21 | ROS2 URDF model | 🔲 | balancer.urdf.xacro | `ros2_ws/src/sbr_description/` |
| 22 | ROS2 Gazebo simulation | 🔲 | Phase 5 optional | — |
| 23 | ROS2 TF2 coordinate transform | 🔲 | base_link → imu → laser | URDF joints |

---

## Status Key

| Symbol | Meaning |
|--------|---------|
| 🔲 | Not started |
| 🔄 | In progress |
| ✅ | Complete and validated |
| ⚠️ | Blocked — see notes |
| ⏭️ | Deferred to later phase |
