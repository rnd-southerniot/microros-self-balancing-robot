# Claude Code — Hardware Bring-Up Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
All 6 software phases are complete. Your job in this session is
hardware bring-up: getting the physical robot running, calibrated,
balanced, and validated end-to-end.

Read this entire prompt before taking any action.

---

## FULL PROJECT STATE

Repository: https://github.com/rnd-southerniot/microros-self-balancing-robot
All software is committed. 8 commits. Everything builds. No code to write
unless hardware validation reveals a bug — treat all code changes as fixes,
not new features.

### Key files you will use constantly

  docs/hardware/pin-map.md                  ← wiring reference (verified)
  docs/hardware-activation-checklist.md     ← your ordered work plan
  docs/software/pid-tuning.md               ← full PID tuning procedure
  firmware/stm32/include/config/robot_params.h  ← PID gains, zero angle
  firmware/stm32/include/config/pin_config.h    ← all GPIO assignments
  scripts/check_topics.py                   ← validation gate tool

### Hardware constants (do not change without physical verification)

  ICM-20948 → I2C3 (PA8=SCL, PC9=SDA). NOT I2C1.
  PB7 → TIM4_CH2 (Motor 2 PWM). Exclusively.
  Encoder → EXTI quadrature on PC4/PC5 (M1) and PB8/PB9 (M2).
  UART STM32↔ESP32 → USART1 PA9(TX)/PA10(RX) @ 921600 baud.
  microROS transport → raw XRCE-DDS over DMA UART (no CRC framing).

### Build system

  STM32:  cd firmware/stm32 && pio run -e stm32f429 -t upload
  ESP32:  cd firmware/esp32 && pio run -e esp32-bridge -t upload
  ROS2:   cd ros2_ws && colcon build --symlink-install
  Tests:  cd firmware/stm32 && pio test -e native   (34/34 must stay green)

  UROS_ENABLED activates automatically when lib/microros/libmicroros.a exists.
  If libmicroros.a is missing: bash firmware/stm32/scripts/build_microros_lib.sh

---

## YOUR WORK — ORDERED SECTIONS

Work through sections in order. Do not skip ahead.
Each section has a gate condition. Do not start the next section
until the gate passes.

---

## SECTION 1 — Base system activation

### What you do

1.1 Build microROS static library (Pi 5, Docker required, ~10 min first run):
  cd firmware/stm32
  bash scripts/build_microros_lib.sh
  ls -lh lib/microros/libmicroros.a   # must exist

1.2 Build ROS2 workspace on Pi 5:
  bash scripts/setup_pi5.sh           # if not already done
  cd ros2_ws
  source /opt/ros/jazzy/setup.bash
  rosdep install --from-paths src --ignore-src -r -y
  colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
  source install/setup.bash

1.3 Flash STM32 (UROS_ENABLED mode):
  cd firmware/stm32
  pio run -e stm32f429                # verify UROS_ENABLED in output (~240KB)
  pio run -e stm32f429 -t upload

1.4 Provision and flash ESP32:
  cd firmware/esp32
  python3 scripts/provision_nvs.py \
    --port /dev/ttyUSB0 \
    --ssid "YOUR_SSID" --password "YOUR_PASS" \
    --agent-ip "PI5_IP_ADDRESS"
  pio run -e esp32-bridge -t upload

1.5 Start microROS agent on Pi 5:
  docker compose -f docker/docker-compose.yml up agent
  # Watch for: "New client connected. Client ID: 1"

1.6 Launch full ROS2 stack:
  source /opt/ros/jazzy/setup.bash && source ros2_ws/install/setup.bash
  ros2 launch sbr_bringup robot.launch.py

### Gate 1 — must pass before Section 2

  python3 scripts/check_topics.py   # all PASS

  Individually verify:
  ros2 topic hz /imu/data            # ≥ 100 Hz
  ros2 topic hz /odom                # ≥ 50 Hz
  ros2 topic hz /odometry/filtered   # ≥ 40 Hz
  ros2 topic echo /balance_state --once   # fault_flags = 0

### Common issues in Section 1

  "Waiting for agent..." forever
    → Check ESP32 WiFi status (serial monitor). Verify agent IP in NVS.
    → Verify ESP32 and Pi 5 are on same WiFi network.

  UROS_DISABLED still showing after lib build
    → Verify lib/microros/libmicroros.a exists and is non-zero size.
    → Re-run: pio run -e stm32f429 (clean build picks up library).

  colcon build fails on sbr_interfaces
    → Build order issue. Run:
       colcon build --packages-select sbr_interfaces
       source install/setup.bash
       colcon build --symlink-install

  /odometry/filtered not publishing
    → ekf_filter_node not started. Check robot.launch.py includes ekf node.
    → Verify ekf_params.yaml path resolution in launch file.

---

## SECTION 2 — IMU validation and zero-angle calibration

### Prerequisite: Gate 1 passed

### What you do

2.1 Verify IMU axes — robot STATIONARY and FLAT on table:
  ros2 topic echo /imu/data --once

  Expected values:
    linear_acceleration.z ≈ 9.81 m/s²   (gravity)
    linear_acceleration.x ≈ 0
    linear_acceleration.y ≈ 0
    angular_velocity.x/y/z ≈ 0

  Tilt robot nose-up ~20°:
    /balance_state pitch_deg must INCREASE (positive = nose up)

  If pitch DECREASES when nose goes up:
    → IMU axis sign is inverted.
    → In firmware/stm32/src/imu/imu_filter.c: negate gyro_y and accel_x.
    → Reflash. Re-verify.
    → Update test_imu_filter.c to match new sign convention.
    → Run: pio test -e native   (all 34 tests must still pass)

  Document confirmed axis convention in docs/hardware/pin-map.md.

2.2 Calibrate zero angle:
  python3 tools/calibration/calibrate_imu.py

  OR manually:
  → Hold robot exactly vertical (use a spirit level)
  → ros2 topic echo /balance_state | grep pitch_deg
  → Note the steady-state value (e.g. 2.3°)
  → Update in firmware/stm32/include/config/robot_params.h:
       #define IMU_ZERO_ANGLE_DEG   2.3f   (your measured value)
  → Reflash: pio run -e stm32f429 -t upload
  → Verify: pitch_deg reads ≈ 0° when robot is vertical

### Gate 2 — must pass before Section 3

  [ ] /imu/data accel_z ≈ 9.81 at rest
  [ ] pitch_deg ≈ 0° when robot is vertical
  [ ] pitch_deg increases when robot tilts nose-up
  [ ] IMU_ZERO_ANGLE_DEG committed to robot_params.h

---

## SECTION 3 — Motor and encoder validation

### Prerequisite: Gate 2 passed

### What you do

3.1 Motor direction check:
  ros2 launch sbr_bringup teleop.launch.py
  # Press 'i' key (forward command)
  # Set cmd_vel.linear.x to a small positive value (~0.1 m/s)

  Expected: BOTH wheels spin FORWARD
    (away from you if the robot is facing away from you)

  If Motor 1 spins backward:
    → In firmware/stm32/include/config/pin_config.h:
       Swap MOTOR1_DIR_A_PIN and MOTOR1_DIR_B_PIN defines.
    → Reflash and re-verify.

  If Motor 2 spins backward:
    → Swap MOTOR2_DIR_A_PIN and MOTOR2_DIR_B_PIN.
    → Reflash and re-verify.

  Document confirmed wiring in docs/hardware/pin-map.md.

3.2 Encoder direction check:
  → Roll robot forward by hand ~30 cm on flat surface.
  ros2 topic echo /odom --once | grep position
  # x value must be POSITIVE

  If x is negative:
    → In firmware/stm32/src/motor/encoder.c: negate encoder tick count.
    → Reflash and re-verify.

3.3 Encoder counts sanity check:
  → Roll robot exactly 1 wheel circumference forward by hand.
  → Wheel circumference = 2π × 0.0335m ≈ 0.211m
  → /odom position.x should read ≈ 0.211m
  → If significantly off: check ENCODER_CPR value in robot_params.h (should be 1320)

### Gate 3 — must pass before Section 4

  [ ] Both wheels spin forward on positive cmd_vel
  [ ] Odom x increases when robot moves forward
  [ ] ~0.21m odom change per wheel revolution

---

## SECTION 4 — PID tuning

### Prerequisite: Gate 3 passed. Robot on test stand with wheels free.

### IMPORTANT: Read docs/software/pid-tuning.md in full before starting.

### Tuning order — NEVER skip steps or tune out of order

  1. Angle loop (inner)  — wheels free on test stand
  2. Velocity loop (outer) — flat floor
  3. Steering loop — flat floor

### 4.1 Angle loop (inner) — test stand

Setup:
  → Robot on test stand, wheels completely free (not touching ground)
  → Disable outer loop: set in robot_params.h:
       #define PID_VEL_KP  0.0f
       #define PID_VEL_KI  0.0f
       #define PID_VEL_KD  0.0f
  → Reflash

Monitor live (open rqt_plot):
  ros2 run rqt_plot rqt_plot /pid/angle/error /pid/angle/output /pid/pitch_deg

Tune via service — no reflash needed between gain changes:
  ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
    "{loop_id: 0, kp: 25.0, ki: 0.0, kd: 0.8}"

  loop_id: 0=angle, 1=velocity, 2=steering, 3=speed

Tuning procedure:
  Step 1: Start with KP=10, KD=0. Increase KP until fast oscillation begins.
  Step 2: Note oscillation KP. Set KP = 50% of that value.
  Step 3: Increase KD until oscillation is damped (smooth convergence).
  Step 4: Only if robot drifts from 0° at rest: add tiny KI (start at 0.1).

If robot falls immediately:
  → Check pitch sign (Section 2 gate must be verified first)
  → Check motor direction (Section 3 gate must be verified first)
  → Check /balance_state/fault_flags == 0 (non-zero means IMU or motor fault)
  → Start with very low KP (5.0) and increase slowly

Gate 4A: Robot holds vertical ±2° for 30+ seconds on test stand.

### 4.2 Velocity loop (outer) — flat floor

Setup:
  → Remove from test stand — robot on flat floor
  → Re-enable velocity loop in robot_params.h:
       #define PID_VEL_KP  2.5f
       #define PID_VEL_KI  0.3f
  → Reflash

Monitor:
  ros2 run rqt_plot rqt_plot /pid/vel/error /pid/vel/output /pid/velocity_mps

Tune via service:
  ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
    "{loop_id: 1, kp: 2.5, ki: 0.3, kd: 0.0}"

Tuning procedure:
  → Start with KP=2.5, KI=0.3.
  → If robot drifts slowly in one direction: increase KI slightly.
  → If robot oscillates forward-backward: reduce KP.
  → If robot overcorrects and falls: reduce both KP and KI.

Gate 4B: Robot balances unsupported ≥ 30 seconds on flat floor.

### 4.3 Steering loop (after 4B passes)

  ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
    "{loop_id: 2, kp: 3.0, ki: 0.0, kd: 0.1}"

  Send angular.z commands via teleop. Robot should turn in place.
  Adjust KP until responsive but not oscillating on turns.

### 4.4 Commit tuned gains

  # Update robot_params.h with all tuned values, then:
  git add firmware/stm32/include/config/robot_params.h
  git commit -m "tune(stm32): PID gains validated on hardware

  Angle:    KP=<val> KI=<val> KD=<val>
  Velocity: KP=<val> KI=<val>
  Steering: KP=<val> KD=<val>
  IMU_ZERO_ANGLE_DEG=<val>"

### Gate 4 — must pass before Section 5 and 6

  [ ] Robot balances unsupported ≥ 30 seconds
  [ ] Teleop drives forward, backward, and turns correctly
  [ ] cmd_vel watchdog: kill teleop → robot stops within 600ms
  [ ] python3 scripts/check_topics.py — all PASS
  [ ] Gains committed to robot_params.h

---

## SECTION 5 — LiDAR + SLAM + Nav2

### Prerequisite: Gate 4 passed. YDLIDAR X4 connected to Pi 5 USB.

5.1 LiDAR setup:
  sudo apt install ros-jazzy-ydlidar-ros2-driver
  sudo cp /opt/ros/jazzy/share/ydlidar_ros2_driver/startup/udev/*.rules \
       /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
  ls /dev/ydlidar           # must exist

  ros2 launch sbr_bringup lidar.launch.py
  ros2 topic hz /scan       # must be ≥ 5 Hz

  Verify laser_frame TF:
  ros2 run tf2_tools view_frames
  # laser_frame must appear as child of base_link

  If scan frame_id is not "laser_frame":
  → Edit ros2_ws/src/sbr_bringup/config/lidar_params.yaml
  → Set frame_id: laser_frame
  → colcon build --packages-select sbr_bringup

5.2 First mapping session:
  # Launch all in separate terminals:
  ros2 launch sbr_bringup robot.launch.py
  ros2 launch sbr_bringup lidar.launch.py
  ros2 launch sbr_bringup slam.launch.py
  ros2 launch sbr_bringup teleop.launch.py

  # On Host VM — open RVIZ2:
  # Add displays: Map, LaserScan, RobotModel, TF
  # Drive robot around the room with teleop

  When map looks complete:
  ros2 run nav2_map_server map_saver_cli -f ros2_ws/maps/room_map
  git add ros2_ws/maps/room_map.yaml
  git commit -m "feat(maps): first room map saved"

5.3 Autonomous navigation:
  ros2 launch sbr_bringup robot.launch.py
  ros2 launch sbr_bringup lidar.launch.py
  ros2 launch sbr_bringup localization.launch.py map:=ros2_ws/maps/room_map.yaml
  ros2 launch sbr_bringup nav2.launch.py

  In RVIZ2:
  → Click "2D Pose Estimate" — click on robot's location on map
  → Click "Nav2 Goal" — click destination on map
  → Robot should navigate without falling

  CRITICAL: Nav2 is configured to publish to /cmd_vel_nav (not /cmd_vel).
  twist_mux arbitrates. Do NOT change this — it prevents Nav2 from
  sending full-speed commands that topple the balance robot.

### Gate 5

  [ ] /scan ≥ 5 Hz
  [ ] laser_frame in TF tree
  [ ] Map builds without distortion while driving
  [ ] Robot navigates to Nav2 goal without falling
  [ ] Obstacle avoidance active

---

## SECTION 6 — Vision

### Prerequisite: Gate 4 passed (independent of Section 5)

6.1 Camera bring-up:
  # CSI camera (Pi 5 preferred)
  libcamera-hello --list-cameras
  sudo apt install python3-picamera2 ros-jazzy-image-pipeline

  # USB camera (alternative)
  ls /dev/video*

  ros2 launch sbr_bringup vision.launch.py
  ros2 topic hz /camera/image_raw   # ≥ 15 Hz

6.2 Camera calibration:
  ros2 run camera_calibration cameracalibrator \
    --size 8x6 --square 0.025 \
    image:=/camera/image_raw camera:=/camera
  # Move output to ros2_ws/src/sbr_bringup/config/camera_calibration.yaml

  Update camera_joint in balancer.urdf.xacro if physical mounting differs
  from the placeholder position. colcon build after any URDF change.

6.3 QR and face tracking:
  pip install pyzbar mediapipe
  sudo apt install libzbar0

  ros2 launch sbr_bringup vision.launch.py enable_qr:=true enable_face:=true
  ros2 topic echo /qr_code/data         # hold QR code in front of camera
  ros2 topic echo /face/detections      # verify face detected

  Enable face tracking (steers robot toward face — angular.z only):
  ros2 param set /face_track_node enable_tracking true

  Monitor CPU temp — must stay below 80°C:
  vcgencmd measure_temp

### Gate 6

  [ ] /camera/image_raw ≥ 15 Hz
  [ ] QR detection works on printed QR code
  [ ] Face tracking steers robot (does not drive forward)
  [ ] CPU temp < 80°C under full load

---

## FINAL VALIDATION AND RELEASE

After all gates pass:

  python3 scripts/check_topics.py     # all PASS
  bash scripts/record_session.sh      # verify bag records cleanly
  pio test -e native                  # 34/34 still pass

  git tag -a v1.0.0 -m "v1.0.0: All 6 phases hardware-validated"
  git push origin main --tags

---

## CODING RULES (if you need to fix any bugs)

  All constants → robot_params.h. All pins → pin_config.h.
  No hardcoded IPs or credentials anywhere.
  Commit: "fix(stm32): ...", "fix(ros2): ...", "tune(stm32): ..."
  After any firmware change: pio test -e native must still pass 34/34.
  After any ROS2 change: colcon test must stay green.
  After any fix: re-verify the gate condition for that section.

---

## ESCALATION — when to come back to claude.ai

Come back to claude.ai (not Claude Code) for:
  - Robot still falls after following full Section 4 debug checklist
  - Unexpected hardware behavior not covered in docs/hardware/pin-map.md
  - Need to redesign part of the firmware (e.g. different IMU fusion algorithm)
  - SLAM map quality problems that don't respond to config tuning
  - Any decision that affects the architecture in docs/software/architecture.md

For those, describe the exact symptom, the rqt_plot trace if relevant,
and which gate you're stuck on.

---

## READ FIRST (in this order)

  1. docs/hardware/pin-map.md
  2. docs/hardware-activation-checklist.md
  3. docs/software/pid-tuning.md
  4. firmware/stm32/include/config/robot_params.h
  5. firmware/stm32/include/config/pin_config.h

Then begin Section 1, Step 1.1.
