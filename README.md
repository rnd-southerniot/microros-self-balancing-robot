# MicroROS Self-Balancing Robot

A production-grade self-balancing robot built on the Yahboom MicroROS SBR platform,
following the official Yahboom lesson packs. All 6 software phases complete.

## Hardware

| Component | Role | Interface |
|-----------|------|-----------|
| STM32F429I-DISC1 | Real-time balance control (200 Hz) | — |
| ICM-20948 | Primary IMU — 9-axis accel/gyro/mag | I2C3 @ 0x68 (PA8/PC9) |
| L3GD20 (onboard Discovery) | Secondary gyro — cross-check | SPI1 |
| 520 motors × 2 + encoder | Drive wheels | TIM3/TIM4 PWM |
| ESP32 | microROS WiFi bridge | UART ↔ UDP |
| HC-SR04 | Obstacle detection | TIM input capture |
| Raspberry Pi 5 | On-robot ROS2 Humble compute | Ethernet / WiFi |
| ILI9341 LCD (onboard) | Status display | SPI5 |
| YDLIDAR X4 | SLAM mapping + obstacle avoidance | USB → /dev/ydlidar |
| CSI / USB camera | Vision module (QR, face tracking) | Pi 5 CSI or USB |

> **Pin note:** ICM-20948 uses I2C3 (PA8=SCL, PC9=SDA) — not I2C1.
> PB7 is reserved exclusively for TIM4_CH2 (Motor 2 PWM).
> Encoder uses EXTI quadrature — hardware TIM encoder mode unavailable on PC4/PC5, PB8/PB9.

## Architecture

```
ICM-20948 (I2C3) ─┐
L3GD20    (SPI1) ─┤─► STM32F429 (200 Hz PID) ─► UART 921600 ─► ESP32
520 motors + enc  ─┘                                              │
HC-SR04           ────────────────────────────────────────────────┘
                                                    WiFi UDP (microROS)
                                                              │
                                                    Raspberry Pi 5
                                              (ROS2 Humble, microROS agent,
                                               Nav2, SLAM Toolbox, EKF,
                                               YDLIDAR, vision nodes)
                                                              │
                                                    Ethernet / WiFi
                                                              │
                                                    Host VM (RVIZ2 — optional)
```

## Repository Structure

```
microros-self-balancing-robot/
├── firmware/
│   ├── stm32/               # STM32F429 PlatformIO — balance control + microROS
│   │   ├── include/config/  # robot_params.h, pin_config.h (single source of truth)
│   │   ├── src/             # imu, motor, pid, balance, uart, uros modules
│   │   └── test/native/     # 34 unit tests — run on x86, no hardware needed
│   └── esp32/               # ESP32 PlatformIO — WiFi UART↔UDP relay
├── ros2_ws/src/
│   ├── sbr_interfaces/      # BalanceState.msg, RobotStatus.msg, SetPidGains.srv
│   │                        # QRDetection.msg, FaceDetection.msg, GestureCommand.msg
│   ├── sbr_description/     # URDF — laser_frame + camera_frame placeholders
│   ├── sbr_bringup/         # All launch files + YAML configs
│   │   └── launch/          # robot, teleop, lidar, slam, localization, nav2, vision
│   ├── sbr_controller/      # safety_node, telemetry_node, pid_monitor_node
│   └── sbr_vision/          # camera_node, qr_node, face_track_node
├── docs/
│   ├── hardware/            # pin-map.md (confirmed wiring + conflict resolutions)
│   ├── software/            # architecture.md, pid-tuning.md
│   ├── lessons/             # Yahboom lesson tracker + phase handoff prompts
│   └── hardware-activation-checklist.md   # ordered bring-up guide
├── tools/calibration/       # calibrate_imu.py
├── scripts/                 # setup_pi5.sh, check_topics.py, record_session.sh
└── docker/                  # microROS agent + ROS2 ARM64 build environment
```

## Quickstart

### Prerequisites

- Raspberry Pi 5 running Ubuntu 24.04
- Docker installed on Pi 5
- STM32 ST-Link programmer
- ESP32 connected via USB

### 1. Bootstrap Pi 5

```bash
bash scripts/setup_pi5.sh
```

### 2. Build microROS static library (Pi 5, first time only — ~10 min)

```bash
cd firmware/stm32
bash scripts/build_microros_lib.sh
```

### 3. Flash STM32

```bash
cd firmware/stm32
pio run -e stm32f429 -t upload    # UROS_ENABLED activates automatically
```

### 4. Provision and flash ESP32

```bash
cd firmware/esp32
python3 scripts/provision_nvs.py \
  --port /dev/ttyUSB0 \
  --ssid "YourSSID" --password "YourPass" \
  --agent-ip "<Pi5-IP-address>"
pio run -e esp32-bridge -t upload
```

### 5. Build ROS2 workspace on Pi 5

```bash
cd ros2_ws
source /opt/ros/humble/setup.bash
colcon build --symlink-install
source install/setup.bash
```

### 6. Start microROS agent

```bash
docker compose -f docker/docker-compose.yml up agent
```

### 7. Launch robot stack

```bash
ros2 launch sbr_bringup robot.launch.py
```

### 8. Teleop

```bash
ros2 launch sbr_bringup teleop.launch.py
```

### 9. Validate all topics

```bash
python3 scripts/check_topics.py
```

## Full Stack Launch Order

```bash
# Terminal 1 — microROS agent
docker compose -f docker/docker-compose.yml up agent

# Terminal 2 — base robot stack
ros2 launch sbr_bringup robot.launch.py

# Terminal 3 — LiDAR (when YDLIDAR X4 connected)
ros2 launch sbr_bringup lidar.launch.py

# Terminal 4 — SLAM mapping OR localization + Nav2
ros2 launch sbr_bringup slam.launch.py
# OR
ros2 launch sbr_bringup localization.launch.py map:=ros2_ws/maps/room_map.yaml
ros2 launch sbr_bringup nav2.launch.py

# Terminal 5 — Teleop
ros2 launch sbr_bringup teleop.launch.py

# Terminal 6 — Vision (when camera connected)
ros2 launch sbr_bringup vision.launch.py enable_qr:=true enable_face:=true
```

## Development

```bash
# STM32: build + native unit tests (no hardware needed)
cd firmware/stm32
pio run -e stm32f429       # build
pio test -e native         # 34/34 tests on x86

# ROS2 workspace
cd ros2_ws && colcon build --symlink-install && colcon test

# PID live tuning (robot running)
ros2 run rqt_plot rqt_plot /pid/angle/error /pid/angle/output /pid/vel/error

# Runtime PID gain update (no reflash required)
ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
  "{loop_id: 0, kp: 25.0, ki: 0.0, kd: 0.8}"

# Record diagnostic session
bash scripts/record_session.sh

# Topic health check
python3 scripts/check_topics.py
```

## ROS2 Topic Reference

| Topic | Type | Rate | Source |
|-------|------|------|--------|
| `/imu/data` | `sensor_msgs/Imu` | 100 Hz | ICM-20948 via STM32 |
| `/odom` | `nav_msgs/Odometry` | 50 Hz | Wheel encoders |
| `/odometry/filtered` | `nav_msgs/Odometry` | 50 Hz | EKF (IMU + odom) |
| `/sonar/range` | `sensor_msgs/Range` | 10 Hz | HC-SR04 |
| `/balance_state` | `sbr_interfaces/BalanceState` | 100 Hz | PID internals |
| `/robot_status` | `sbr_interfaces/RobotStatus` | 1 Hz | System health |
| `/safety/estopped` | `std_msgs/Bool` | 200 Hz | safety_node |
| `/scan` | `sensor_msgs/LaserScan` | 7 Hz | YDLIDAR X4 |
| `/map` | `nav_msgs/OccupancyGrid` | 0.2 Hz | SLAM Toolbox |
| `/camera/image_raw` | `sensor_msgs/Image` | 30 Hz | Pi 5 camera |
| `/qr_code/data` | `std_msgs/String` | on detect | qr_node |
| `/face/detections` | `sbr_interfaces/FaceDetection` | 30 Hz | face_track_node |
| `/cmd_vel` | `geometry_msgs/Twist` | — | twist_mux output |

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `/imu/data` not publishing | STM32 not connecting to agent | Check ESP32 WiFi, verify agent IP in NVS |
| Robot falls immediately | IMU axis sign wrong or zero-angle off | Run `calibrate_imu.py`, check `IMU_ZERO_ANGLE_DEG` |
| Motors spin wrong direction | DIR pin inverted | Swap `MOTOR_DIR_A/B` in `pin_config.h` |
| `colcon build` fails on sbr_interfaces | Build order | `colcon build --packages-select sbr_interfaces` first |
| Nav2 topples robot | cmd_vel not remapped | Verify `cmd_vel_topic: /cmd_vel_nav` in `nav2_params.yaml` |
| YDLIDAR not found | Missing udev rule | Follow udev setup in `docs/hardware/pin-map.md` |
| Camera node crashes on start | No camera connected | Expected — node degrades gracefully |

## Phase Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ | Repo scaffold + CI + Docker |
| 1 | ✅ | STM32 hardware drivers (ICM-20948, motors, encoder, PID, UART) |
| 2 | ✅ | microROS layer (DMA transport, 4 publishers, cmd_vel, SetPidGains) |
| 3 | ✅ | ROS2 workspace (colcon clean, safety/telemetry nodes, teleop) |
| 4B | ✅ | Observability (pid_monitor_node, rosbag, check_topics, README) |
| 4A | 🔄 | PID tuning on hardware (needs physical robot on test stand) |
| 5 | ✅ code / 🔄 hw | LiDAR + SLAM + Nav2 (needs YDLIDAR X4 kit) |
| 6 | ✅ code / 🔄 hw | Vision — QR, face tracking, gesture (needs camera on Pi 5) |

**Hardware activation sequence:** `docs/hardware-activation-checklist.md`

## Key Documents

| Document | Purpose |
|----------|---------|
| `docs/hardware/pin-map.md` | Confirmed pin table + conflict resolutions |
| `docs/software/architecture.md` | System diagram, topic graph, UART protocol |
| `docs/software/pid-tuning.md` | Step-by-step PID tuning procedure |
| `docs/hardware-activation-checklist.md` | Ordered bring-up guide for physical robot |
| `docs/lessons/yahboom-lesson-tracker.md` | All Yahboom lessons mapped to repo files |

## Contact

Technical support: support@yahboom.com
Yahboom lessons: https://www.yahboom.net/study/SBR-microROS
