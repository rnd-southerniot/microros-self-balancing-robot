# Hardware Activation Checklist

All software is written and tested. This document is the ordered
sequence to bring the physical robot to full operation.

Work through sections in order. Each section has a gate condition
that must pass before the next section begins.

---

## Section 1 — Base system activation

**Prerequisite:** Robot physically assembled, Pi 5 powered, ESP32 wired
to STM32 (UART PA9/PA10), all motor/encoder/IMU connections verified
against `docs/hardware/pin-map.md`.

### 1.1 — Build microROS static library

On Raspberry Pi 5 (requires Docker):

```bash
cd firmware/stm32
bash scripts/build_microros_lib.sh
ls -lh lib/microros/libmicroros.a     # must exist
```

Expected build time: 5–15 minutes first run.

### 1.2 — Build ROS2 workspace on Pi 5

```bash
bash scripts/setup_pi5.sh             # if not already done
cd ros2_ws
source /opt/ros/humble/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
source install/setup.bash
```

### 1.3 — Flash STM32 with UROS_ENABLED firmware

```bash
cd firmware/stm32
pio run -e stm32f429                  # verify UROS_ENABLED in build output
pio run -e stm32f429 -t upload        # flash via ST-Link
```

Expected Flash usage: ~240 KB (was 19.8 KB in DISABLED mode).

### 1.4 — Start microROS agent and verify connection

```bash
docker compose -f docker/docker-compose.yml up agent
```

Watch agent log. After ESP32 connects and STM32 boots:
```
New client connected. Client ID: 1
```

### 1.5 — Verify base topics

```bash
source /opt/ros/humble/setup.bash && source ros2_ws/install/setup.bash
ros2 topic hz /imu/data               # ≥ 100 Hz
ros2 topic hz /odom                   # ≥ 50 Hz
ros2 topic echo /balance_state --once # fault_flags should be 0
```

### 1.6 — Launch full ROS2 stack

```bash
ros2 launch sbr_bringup robot.launch.py
```

Verify all nodes running:
```bash
ros2 node list
# /micro_ros_agent, /robot_state_publisher, /ekf_filter_node,
# /safety_node, /telemetry_node, /twist_mux, /pid_monitor_node
ros2 topic hz /odometry/filtered      # ≥ 40 Hz
```

**Gate 1:** `python3 scripts/check_topics.py` — all PASS

---

## Section 2 — IMU validation and zero-angle calibration

**Prerequisite:** Gate 1 passed.

### 2.1 — Verify IMU axes (robot stationary, flat on table)

```bash
ros2 topic echo /imu/data --once
```

Expected:
- `linear_acceleration.z ≈ 9.81 m/s²`
- `linear_acceleration.x ≈ 0`, `.y ≈ 0`
- All `angular_velocity` ≈ 0

```bash
ros2 topic echo /balance_state/pitch_deg --once   # ≈ 0° when flat
```

Tilt robot nose-up 20°:
- `/balance_state/pitch_deg` must **increase** (positive = nose up)
- If it decreases: negate `gyro_y` and `accel_x` in `src/imu/imu_filter.c`

### 2.2 — Calibrate zero angle

```bash
python3 tools/calibration/calibrate_imu.py
```

Or manually:
1. Hold robot vertical (use a spirit level)
2. Note steady-state value from `/balance_state/pitch_deg`
3. Update `IMU_ZERO_ANGLE_DEG` in `firmware/stm32/include/config/robot_params.h`
4. Reflash and verify pitch reads ≈ 0° at vertical

**Gate 2:** Pitch reads ≈ 0° at vertical, increases when nose tilts up.

---

## Section 3 — Motor and encoder validation

**Prerequisite:** Gate 2 passed.

### 3.1 — Motor direction check

Apply a small positive velocity via teleop:
```bash
ros2 launch sbr_bringup teleop.launch.py
# Press 'i' (forward)
```

Both wheels must spin **forward** (away from you if robot faces away).
If either wheel spins backward: swap `MOTOR_DIR_A` / `MOTOR_DIR_B`
for that motor in `pin_config.h`. Reflash.

### 3.2 — Encoder direction check

Roll robot forward by hand 30 cm:
```bash
ros2 topic echo /odom/pose/pose/position/x --once
```

Value must be **positive**. If negative: negate encoder count in
`src/motor/encoder.c`. Reflash.

**Gate 3:** Forward motion = positive odom x, both wheels spin forward.

---

## Section 4 — PID tuning on test stand

**Prerequisite:** Gate 3 passed. Robot on test stand with wheels free.

Read `docs/software/pid-tuning.md` in full before starting.

### 4.1 — Angle (inner) loop

Disable outer loop: set `PID_VEL_KP=0 KI=0 KD=0` in `robot_params.h`, reflash.

Monitor live:
```bash
ros2 run rqt_plot rqt_plot /pid/angle/error /pid/angle/output
```

Tune via service (no reflash):
```bash
ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
  "{loop_id: 0, kp: 25.0, ki: 0.0, kd: 0.8}"
```

**Gate 4A:** Robot holds ±2° for 30+ seconds on test stand.

### 4.2 — Velocity (outer) loop

Re-enable: `PID_VEL_KP=2.5, KI=0.3` in `robot_params.h`, reflash.
Remove from test stand — flat floor.

```bash
ros2 run rqt_plot rqt_plot /pid/vel/error /pid/vel/output
```

**Gate 4B:** Robot balances unsupported ≥ 30 seconds on flat floor.

### 4.3 — Commit tuned gains

```bash
# Update firmware/stm32/include/config/robot_params.h with tuned values
git add firmware/stm32/include/config/robot_params.h
git commit -m "tune(stm32): PID gains validated on hardware

Angle: KP=<val> KI=<val> KD=<val>
Velocity: KP=<val> KI=<val>
Steering: KP=<val> KD=<val>
IMU_ZERO_ANGLE_DEG=<val>"
```

**Gate 4:** `python3 scripts/check_topics.py` all PASS + robot balances.

---

## Section 5 — LiDAR + SLAM + Nav2

**Prerequisite:** Gate 4 passed. YDLIDAR X4 received and connected to Pi 5 USB.

### 5.1 — LiDAR setup

```bash
# Set up udev rule so device appears as /dev/ydlidar
sudo cp /opt/ros/humble/share/ydlidar_ros2_driver/startup/udev/*.rules \
     /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
ls /dev/ydlidar   # must exist

sudo apt install ros-humble-ydlidar-ros2-driver
ros2 launch sbr_bringup lidar.launch.py
ros2 topic hz /scan                   # ≥ 5 Hz
```

### 5.2 — Build first map

```bash
ros2 launch sbr_bringup robot.launch.py
ros2 launch sbr_bringup lidar.launch.py
ros2 launch sbr_bringup slam.launch.py
ros2 launch sbr_bringup teleop.launch.py   # drive around room
# On Host VM: open RVIZ2, add Map + LaserScan displays
```

When map looks complete:
```bash
ros2 run nav2_map_server map_saver_cli -f ros2_ws/maps/room_map
git add ros2_ws/maps/room_map.yaml
git commit -m "feat(maps): first room map saved"
```

### 5.3 — Autonomous navigation

```bash
ros2 launch sbr_bringup robot.launch.py
ros2 launch sbr_bringup lidar.launch.py
ros2 launch sbr_bringup localization.launch.py map:=ros2_ws/maps/room_map.yaml
ros2 launch sbr_bringup nav2.launch.py
# In RVIZ2: set 2D Pose Estimate, then Nav2 Goal
```

**Gate 5:** Robot navigates to goal without falling. Obstacle avoidance active.

---

## Section 6 — Vision

**Prerequisite:** Gate 4 passed (vision is independent of LiDAR).
CSI or USB camera attached to Pi 5.

### 6.1 — Camera bring-up

```bash
# CSI
libcamera-hello --list-cameras
# USB
ls /dev/video*

sudo apt install ros-humble-image-pipeline python3-opencv python3-picamera2
ros2 launch sbr_bringup vision.launch.py
ros2 topic hz /camera/image_raw       # ≥ 15 Hz
```

### 6.2 — Calibrate camera

```bash
ros2 run camera_calibration cameracalibrator --size 8x6 --square 0.025 \
  image:=/camera/image_raw camera:=/camera
# Move calibration file to ros2_ws/src/sbr_bringup/config/camera_calibration.yaml
```

### 6.3 — QR code and face tracking

```bash
pip install pyzbar mediapipe
sudo apt install libzbar0

ros2 launch sbr_bringup vision.launch.py enable_qr:=true enable_face:=true
ros2 topic echo /qr_code/data          # hold QR code in front of camera
ros2 topic echo /face/detections       # face detection active
```

Enable face tracking (steers robot toward face):
```bash
ros2 param set /face_track_node enable_tracking true
```

**Gate 6:** QR detects, face tracking steers, CPU temp < 80°C.

---

## Final validation

Run after all sections complete:

```bash
python3 scripts/check_topics.py       # all PASS
bash scripts/record_session.sh        # verify bag records
ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
  "{loop_id: 0, kp: 25.0, ki: 0.0, kd: 0.8}"   # service responds
```

Commit any final adjustments and tag the release:

```bash
git tag -a v1.0.0 -m "v1.0.0: All 6 phases hardware-validated"
git push origin main --tags
```

---

## Quick reference — full stack launch order

```bash
# Terminal 1 — microROS agent
docker compose -f docker/docker-compose.yml up agent

# Terminal 2 — base robot stack
ros2 launch sbr_bringup robot.launch.py

# Terminal 3 — LiDAR (when available)
ros2 launch sbr_bringup lidar.launch.py

# Terminal 4 — SLAM or Nav2
ros2 launch sbr_bringup slam.launch.py             # mapping
ros2 launch sbr_bringup localization.launch.py map:=maps/room_map.yaml
ros2 launch sbr_bringup nav2.launch.py             # navigation

# Terminal 5 — Teleop
ros2 launch sbr_bringup teleop.launch.py

# Terminal 6 — Vision (when camera available)
ros2 launch sbr_bringup vision.launch.py enable_qr:=true

# Any terminal — Health check
python3 scripts/check_topics.py
bash scripts/record_session.sh
```
