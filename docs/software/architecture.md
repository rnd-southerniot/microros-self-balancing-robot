# Software Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│               Host VM (Ubuntu 22.04)                    │
│    RVIZ2 · Foxglove · SSH dev · remote teleop           │
│    (optional — not required on robot)                   │
└───────────────────────┬─────────────────────────────────┘
                        │ Ethernet / WiFi (ROS2 DDS)
┌───────────────────────▼─────────────────────────────────┐
│            Raspberry Pi 5 — ROS2 Humble                 │
│                                                         │
│  microROS agent (UDP:8888)   robot_localization EKF     │
│  Nav2 stack                  twist_mux                  │
│  SLAM Toolbox (Phase 5)      safety_node                │
│  sbr_controller              telemetry_node             │
└───────────────────────┬─────────────────────────────────┘
                        │ WiFi UDP (microROS)
┌───────────────────────▼─────────────────────────────────┐
│              ESP32 — microROS bridge                    │
│                                                         │
│   UART RX → UDP TX (upstream to Pi 5 agent)             │
│   UDP RX  → UART TX (downstream to STM32)               │
│   Credentials: NVS flash (no hardcode)                  │
└───────────────────────┬─────────────────────────────────┘
                        │ UART 921600 (binary framed)
┌───────────────────────▼─────────────────────────────────┐
│         STM32F429I-DISC1 — Real-time control            │
│                                                         │
│   200 Hz:  IMU read → complementary filter              │
│            angle PID → velocity PID → motor PWM         │
│   50 Hz:   encoder odometry calculation                 │
│   100 Hz:  microROS publishers (via ESP32 relay)        │
│   Inputs:  ICM-20948 (I2C1) + L3GD20 (SPI1)            │
│   Outputs: TIM3/TIM4 PWM, H-bridge GPIO, LCD, UART      │
└───────────────────────┬─────────────────────────────────┘
                        │
         ┌──────────────┼──────────────┐
         ▼              ▼              ▼
   520 motors      ICM-20948      HC-SR04
   + encoders      9-axis IMU     ultrasonic
```

---

## ROS2 Topic Graph

### Published by STM32 → ESP32 → Pi 5

| Topic | Type | Rate | Source |
|-------|------|------|--------|
| `/imu/data` | `sensor_msgs/Imu` | 100 Hz | ICM-20948 |
| `/odom` | `nav_msgs/Odometry` | 50 Hz | Wheel encoders |
| `/sonar/range` | `sensor_msgs/Range` | 10 Hz | HC-SR04 |
| `/balance_state` | `sbr_interfaces/BalanceState` | 100 Hz | PID internals |
| `/robot_status` | `sbr_interfaces/RobotStatus` | 1 Hz | STM32 health |

### Subscribed by STM32 ← ESP32 ← Pi 5

| Topic | Type | Source |
|-------|------|--------|
| `/cmd_vel` | `geometry_msgs/Twist` | twist_mux output |

### Published by Pi 5 nodes

| Topic | Type | Node | Rate |
|-------|------|------|------|
| `/odometry/filtered` | `nav_msgs/Odometry` | robot_localization EKF | 50 Hz |
| `/safety/estopped` | `std_msgs/Bool` | safety_node | 200 Hz |
| `/tf` | `tf2_msgs/TFMessage` | robot_state_publisher | on change |

---

## Control Loop Architecture

### STM32 cascade PID (200 Hz)

```
cmd_vel.linear.x
       │
       ▼
┌─────────────────┐
│  Velocity PID   │  (outer loop)
│  kp=2.5 ki=0.3  │
└────────┬────────┘
         │ pitch setpoint offset
         ▼
┌─────────────────┐     ICM-20948
│   Angle PID     │ ◄── complementary filter
│  kp=25 kd=0.8   │     pitch estimate
└────────┬────────┘
         │ motor torque command
         ▼
┌─────────────────┐
│  Motor driver   │ ──► TIM3/TIM4 PWM
│  + H-bridge     │     (PB4, PB7)
└─────────────────┘

cmd_vel.angular.z
       │
       ▼
┌─────────────────┐
│  Steering PID   │ ──► differential torque added to both motors
│  kp=3.0 kd=0.1  │
└─────────────────┘
```

### IMU fusion (500 Hz, complementary filter)

```
ICM-20948 gyro_y  ──► integrate:  angle += gyro_y * dt          (α = 0.98)
ICM-20948 accel   ──► arctan:     accel_angle = atan2(ax, az)   (1-α = 0.02)

pitch = α * (pitch + gyro_rate * dt) + (1-α) * accel_angle

Cross-check:
L3GD20 gyro_z  ──► |ICM20948.gyro_z - L3GD20.gyro_z| > 5°/s → IMU_FAULT_DIVERGED
```

---

## UART Frame Protocol (STM32 ↔ ESP32)

Binary framing to minimise latency and overhead:

```
┌──────────┬──────────┬──────────┬─────────────────┬──────────┬──────────┐
│ START    │ MSG_TYPE │ LENGTH   │ PAYLOAD         │ CRC16    │ END      │
│ 0xAA     │ 1 byte   │ 1 byte   │ 0..63 bytes     │ 2 bytes  │ 0x55     │
└──────────┴──────────┴──────────┴─────────────────┴──────────┴──────────┘
```

| MSG_TYPE | Direction | Payload |
|----------|-----------|---------|
| 0x01 | STM32 → ESP32 | IMU data (pitch, rates) |
| 0x02 | STM32 → ESP32 | Odometry (left/right RPM, pose delta) |
| 0x03 | STM32 → ESP32 | Sonar range |
| 0x04 | STM32 → ESP32 | Balance state (PID internals + faults) |
| 0x10 | ESP32 → STM32 | cmd_vel (linear.x, angular.z) |
| 0x11 | ESP32 → STM32 | PID gain update (SetPidGains response) |

---

## Raspberry Pi 5 Node Graph

```
/imu/data ──────────────────────────┐
                                    ▼
/odom ──────────────────────► ekf_filter_node ──► /odometry/filtered
                                                          │
/imu/data ──────────────────► safety_node                │
                               │                         ▼
                               └──► /cmd_vel (zero)   Nav2 (Phase 5)
                               └──► /safety/estopped        │
                                                            ▼
/cmd_vel_teleop ─────────────┐                        /cmd_vel_nav
/cmd_vel_nav ────────────────┤                              │
                             ▼                             │
                        twist_mux ──────────────────► /cmd_vel
                                                          │
                                               ┌──────────┘
                                               │
                                    ESP32 ◄── UDP ◄── Pi 5 agent
                                               │
                                           STM32 ◄── UART
```

---

## Security

| Concern | Mitigation |
|---------|-----------|
| WiFi credentials | Stored in ESP32 NVS flash — never in source |
| ROS2 DDS discovery | `ROS_DOMAIN_ID=0` — isolated network segment recommended |
| SSH access | Key-based auth on Pi 5 — disable password auth |
| rosbag data | Logged to tmpfs by default — not persisted on SD card |

---

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Balance loop rate | 200 Hz | STM32 timer ISR |
| IMU sample rate | 500 Hz | ICM-20948 ODR |
| microROS IMU pub rate | ≥ 100 Hz | `ros2 topic hz /imu/data` |
| WiFi round-trip latency | < 20 ms | ping ESP32 → Pi 5 |
| cmd_vel to motor response | < 10 ms | scope on PWM output |
| EKF output rate | 50 Hz | `ros2 topic hz /odometry/filtered` |
