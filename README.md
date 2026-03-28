# MicroROS Self-Balancing Robot

A production-grade self-balancing robot built on the Yahboom MicroROS SBR platform, following the official Yahboom lesson packs. Built from scratch with a focus on reliability, observability, and reproducibility.

## Hardware

| Component | Role | Interface |
|-----------|------|-----------|
| STM32F429I-DISC1 | Real-time balance control (200 Hz) | — |
| ICM-20948 | Primary IMU — 9-axis accel/gyro/mag | I2C1 @ 0x68 |
| L3GD20 (onboard Discovery) | Secondary gyro — cross-check / fallback | SPI1 |
| 520 motors × 2 + encoder | Drive wheels | TIM3/TIM4 PWM |
| ESP32 | microROS WiFi bridge | UART ↔ UDP |
| HC-SR04 | Obstacle detection | TIM input capture |
| Raspberry Pi 5 | On-robot ROS2 Humble compute | Ethernet / WiFi |
| ILI9341 LCD (onboard) | Status display | SPI5 |

## Architecture

```
ICM-20948 (I2C1) ─┐
L3GD20    (SPI1) ─┤─► STM32F429 (200 Hz PID) ─► UART 921600 ─► ESP32
520 motors + enc  ─┘                                              │
                                                    WiFi UDP (microROS)
                                                              │
                                                    Raspberry Pi 5
                                              (ROS2 Humble, microROS agent,
                                               Nav2, SLAM, robot_localization)
                                                              │
                                                    Ethernet / WiFi
                                                              │
                                                    Host VM (RVIZ2, dev)
```

## Repository Structure

```
microros-self-balancing-robot/
├── firmware/
│   ├── stm32/          # STM32F429 PlatformIO project — balance control
│   └── esp32/          # ESP32 PlatformIO project — microROS WiFi bridge
├── ros2_ws/src/
│   ├── sbr_interfaces/  # Custom msgs / srvs
│   ├── sbr_description/ # URDF robot model
│   ├── sbr_bringup/     # Launch files + YAML configs
│   └── sbr_controller/  # Safety, EKF, telemetry nodes
├── docs/
│   ├── hardware/        # Pin maps, BOM, wiring
│   ├── software/        # Architecture, ROS2 setup
│   └── lessons/         # Notes keyed to Yahboom lessons
├── tools/
│   ├── calibration/     # IMU bias capture scripts
│   └── flash/           # Flash helper scripts
├── docker/              # Reproducible build environments
└── scripts/             # Host setup, topic health checks
```

## Quickstart

### 1. Flash STM32

```bash
cd firmware/stm32
pio run -e stm32f429 -t upload
```

### 2. Flash ESP32 (provision WiFi first)

```bash
cd firmware/esp32
python3 scripts/provision_nvs.py --port /dev/ttyUSB0 \
  --ssid "YourSSID" --password "YourPass" --agent-ip "192.168.x.x"
pio run -e esp32-bridge -t upload
```

### 3. Start microROS agent on Raspberry Pi 5

```bash
docker compose -f docker/docker-compose.yml up agent
```

### 4. Launch ROS2 stack

```bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch sbr_bringup robot.launch.py
```

### 5. Verify topics

```bash
bash scripts/check_topics.sh
```

## Development

- Build + native unit tests: `cd firmware/stm32 && pio test -e native`
- ROS2 build: `cd ros2_ws && colcon build`
- CI: GitHub Actions runs on every push to `firmware/` or `ros2_ws/`

## Phase Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ Scaffold | Repo + environments |
| 1 | 🔲 | Hardware peripheral validation |
| 2 | 🔲 | STM32 balance core + PID |
| 3 | 🔲 | ESP32 microROS → Pi 5 |
| 4 | 🔲 | ROS2 workspace + teleop |
| 5 | 🔲 | LiDAR + SLAM + Nav2 |
| 6 | 🔲 | Vision module |

## Contact

Technical support: support@yahboom.com  
Yahboom lessons: https://www.yahboom.net/study/SBR-microROS
