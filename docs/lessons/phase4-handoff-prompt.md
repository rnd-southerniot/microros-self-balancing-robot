# Claude Code — Phase 4 Handoff Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
Read this entire prompt before taking any action.

---

## PROJECT STATE SUMMARY

### What is built and committed (Phases 1–3)

firmware/stm32/ — COMPLETE, builds clean
  - ICM-20948 driver (I2C3: PA8/PC9), complementary filter, L3GD20 cross-check
  - Motor PWM (TIM3/TIM4), EXTI encoder, odometry, cascade PID, UART framing
  - microROS layer: DMA transport, 4 publishers, cmd_vel sub, SetPidGains service
  - Dual-mode build: UROS_DISABLED (19.8KB) / UROS_ENABLED (activates with lib)
  - 34/34 native tests passing

ros2_ws/src/ — COMPLETE, all packages correct
  - sbr_interfaces: BalanceState.msg, RobotStatus.msg, SetPidGains.srv
  - sbr_description: balancer.urdf.xacro (with laser_frame placeholder)
  - sbr_bringup: robot.launch.py (xacro fixed), teleop.launch.py, ekf_params.yaml
  - sbr_controller: safety_node.cpp, telemetry_node.cpp (tf2 deps fixed)

### KEY CONSTRAINTS (never change)
  ICM-20948 → I2C3 (PA8=SCL, PC9=SDA). PB7 = TIM4_CH2 (Motor 2 PWM) only.
  Encoder   → EXTI quadrature. HW TIM encoder unavailable on PC4/PC5, PB8/PB9.
  Transport → raw XRCE-DDS over DMA UART (no CRC framing in microROS path).
  Build     → UROS_ENABLED when lib/microros/libmicroros.a present.

### What still needs Pi 5 hardware (not yet done)
  1. bash firmware/stm32/scripts/build_microros_lib.sh   (builds libmicroros.a)
  2. cd ros2_ws && colcon build --symlink-install
  3. pio run -e stm32f429 -t upload                       (UROS_ENABLED firmware)
  4. docker compose -f docker/docker-compose.yml up agent
  5. ros2 launch sbr_bringup robot.launch.py
  6. python3 scripts/check_topics.py                     (Phase 3 gate)

---

## PHASE 4 OBJECTIVE

This phase has two tracks that run in parallel once the robot is
physically assembled and balancing on the bench:

Track A — Hardware activation and balance tuning (requires physical robot)
Track B — Code hardening and observability (can run on Pi 5 alone)

By end of Phase 4:
  - Robot physically assembled, powered, and standing on flat surface
  - Phase 3 hardware gate fully passed (check_topics.py all PASS)
  - Robot balances unsupported for ≥ 30 seconds on flat floor
  - cmd_vel from teleop drives the robot in a straight line and turns
  - PID gains tuned and committed to robot_params.h
  - IMU zero-angle calibrated and committed
  - All diagnostic and observability tools working
  - README.md updated with setup instructions and current status

Yahboom lesson ref:
  ROS2 basic §18 (rqt), §20 (rosbag), SBR balance algorithm lessons

---

## TRACK A — Hardware activation and balance tuning

### A1 — Physical robot assembly checklist

Before powering on, verify these hardware connections match pin_config.h:

  Motor 1 PWM → PB4 (TIM3_CH1)        Motor 2 PWM → PB7 (TIM4_CH2)
  Motor 1 Dir A → PA5                  Motor 2 Dir A → PD4
  Motor 1 Dir B → PA7                  Motor 2 Dir B → PD5
  Motor 1 Enc A → PC4 (EXTI)           Motor 2 Enc A → PB9 (EXTI)
  Motor 1 Enc B → PC5 (EXTI)           Motor 2 Enc B → PB8 (EXTI)
  ICM-20948 SCL → PA8 (I2C3)
  ICM-20948 SDA → PC9 (I2C3)
  ESP32 UART TX → PA9 (STM32 USART1 RX)
  ESP32 UART RX → PA10 (STM32 USART1 TX)

Document any wiring deviations in docs/hardware/pin-map.md before proceeding.

### A2 — IMU axis orientation validation

Before any PID work, physically validate IMU axes:

  1. Place robot flat on a table (level). Boot STM32.
  2. ros2 topic echo /imu/data --once
     → accel_z should be ≈ 9.81 m/s², accel_x ≈ accel_y ≈ 0
     → all gyro axes should be ≈ 0 deg/s at rest

  3. Tilt robot nose-up by ~20 degrees.
     → pitch in /balance_state should increase (positive = nose up)
     → If pitch goes negative when nose-up: negate gyro_y and accel_x
        in src/imu/imu_filter.c, update test_imu_filter.c accordingly

  4. Rotate robot right (yaw). → gyro_z should be positive.

Document confirmed axis conventions in docs/hardware/pin-map.md.

### A3 — IMU zero-angle calibration

The robot may not balance at exactly 0° pitch due to:
  - Centre of mass offset (battery, Pi 5 position)
  - ICM-20948 mounting tilt

Procedure:
  1. Place robot on a flat surface, prop wheels off ground on test stand
  2. Manually hold robot vertical (use a level)
  3. ros2 topic echo /balance_state/pitch_deg  — note steady-state value
  4. Update IMU_ZERO_ANGLE_DEG in robot_params.h to that value
  5. Reflash and verify pitch reads ≈ 0° when robot is vertical

Alternatively use the calibration script:
  python3 tools/calibration/calibrate_imu.py

### A4 — Motor direction validation

Before balance PID, verify motor directions are correct:

  1. Set a small positive cmd_vel.linear.x via teleop
  2. Both wheels should spin FORWARD
  3. If either spins backward: swap DIR_A / DIR_B defines for that
     motor in pin_config.h OR negate the PWM sign in motor_driver.c
  4. Verify encoder counts INCREASE when wheels spin forward
     ros2 topic echo /odom/twist/twist/linear/x  — should be positive

Document in docs/hardware/pin-map.md.

### A5 — Balance PID tuning on test stand

Read docs/software/pid-tuning.md in full before starting.

Tuning order (NEVER skip steps or tune out of order):
  1. Angle loop (inner):  kp first, then kd, then tiny ki
  2. Velocity loop (outer): kp first, then ki
  3. Steering: kp first, then kd

Setup for angle loop tuning:
  - Robot on test stand, wheels free
  - Kill velocity loop: set PID_VEL_KP=0 KI=0 KD=0 in robot_params.h
  - Monitor: rqt_plot /balance_state/pitch_deg /balance_state/pitch_error_deg

Runtime gain adjustment (no reflash needed):
  ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
    "{loop_id: 0, kp: <value>, ki: 0.0, kd: <value>}"

  loop_id: 0=angle, 1=velocity, 2=steering, 3=speed

Angle loop validation gate:
  Robot holds vertical ±2° for 30+ seconds on test stand.

### A6 — Velocity loop tuning on flat floor

Only after A5 gate passes.

  - Remove test stand — robot on flat floor
  - Re-enable velocity loop: set PID_VEL_KP=2.5 KI=0.3 in robot_params.h
  - Push robot slightly forward — it should self-correct
  - Tuning: increase KP until robot corrects quickly without oscillating
  - Add KI only if robot slowly drifts in one direction at rest

Validation gate:
  - Robot balances unsupported for ≥ 30 seconds
  - cmd_vel.linear.x = 0.3 → robot moves forward in a straight line
  - cmd_vel = 0 → robot decelerates and stops without falling

### A7 — Commit tuned values

After validation, update firmware/stm32/include/config/robot_params.h:

  #define PID_ANGLE_KP         <tuned_value>
  #define PID_ANGLE_KI         <tuned_value>
  #define PID_ANGLE_KD         <tuned_value>
  #define PID_VEL_KP           <tuned_value>
  #define PID_VEL_KI           <tuned_value>
  #define PID_STEER_KP         <tuned_value>
  #define PID_STEER_KD         <tuned_value>
  #define IMU_ZERO_ANGLE_DEG   <calibrated_value>

Commit with message: "tune(stm32): PID gains validated on hardware"
Never change committed gains without a git note explaining why.

---

## TRACK B — Code hardening and observability

These can be done independently of hardware tuning.

### B1 — Add /odometry/filtered to check_topics.py

Update scripts/check_topics.py to include:
  ("/odometry/filtered",  40,  60),
  ("/safety/estopped",    10, 250),
  ("/robot_status",       0.5,  2),

### B2 — Add rosbag recording script

Create scripts/record_session.sh:

  #!/usr/bin/env bash
  # Records all diagnostic topics for post-session analysis.
  STAMP=$(date +%Y%m%d_%H%M%S)
  ros2 bag record \
    /imu/data \
    /odom \
    /odometry/filtered \
    /cmd_vel \
    /balance_state \
    /robot_status \
    /safety/estopped \
    -o "bags/session_${STAMP}" \
    --max-bag-size 500000000

Add bags/ to .gitignore.
Yahboom lesson ref: ROS2 basic §20

### B3 — Add pid_monitor_node

Create ros2_ws/src/sbr_controller/src/pid_monitor_node.cpp

This node:
  - Subscribes to /balance_state
  - Re-publishes individual PID terms as Float32 topics for rqt_plot:
      /pid/angle/error    /pid/angle/output
      /pid/vel/error      /pid/vel/output
  - Exposes SetPidGains service as a proxy (forwards to STM32)
  - Rate: 100 Hz passthrough

Register in sbr_controller/CMakeLists.txt and sbr_bringup/robot.launch.py

### B4 — Teleop launch improvements

Update ros2_ws/src/sbr_bringup/launch/teleop.launch.py:
  - Print startup banner with key bindings
  - Add max_vel and max_turn launch arguments
  - Remap clearly: /cmd_vel_teleop → twist_mux priority 20

### B5 — Update README.md

Update README.md with:
  - Current phase status
  - Prerequisites: hardware, software versions
  - Step-by-step quickstart for someone with the assembled robot
  - Link to docs/software/pid-tuning.md for tuning guidance
  - Troubleshooting section for common issues

### B6 — Update lesson tracker

Update docs/lessons/yahboom-lesson-tracker.md:
  - Mark all completed lessons ✅
  - Update phase summary table
  - Write docs/lessons/phase5-handoff-prompt.md for LiDAR + SLAM

---

## VALIDATION GATE — Phase 4 complete when ALL pass

Hardware:
  [ ] IMU axes validated and documented
  [ ] Motor directions verified (forward = positive cmd_vel)
  [ ] Encoder counts increase in forward direction
  [ ] IMU_ZERO_ANGLE_DEG calibrated and committed
  [ ] Robot balances unsupported ≥ 30 seconds on flat floor
  [ ] Teleop drives robot forward, backward, and turns correctly
  [ ] cmd_vel watchdog fires within 600ms — robot stops safely

Software:
  [ ] python3 scripts/check_topics.py — all PASS (including /odometry/filtered)
  [ ] PID gains committed to robot_params.h with git note
  [ ] rosbag recording script working
  [ ] colcon build + colcon test — 0 errors
  [ ] README.md updated with current setup instructions

---

## WHAT TO DO IF ROBOT FALLS IMMEDIATELY

Step-by-step debug checklist (in order):

  1. Check pitch reads ≈ 0° at vertical:
       ros2 topic echo /balance_state/pitch_deg
     → If ±5° or more: re-run A3 (zero angle calibration)

  2. Check IMU axis sign:
       Tilt nose up → pitch_deg should increase positively
     → If inverted: fix axis sign in imu_filter.c

  3. Check motor direction:
       Apply small positive cmd_vel manually
       Both wheels should push robot UPRIGHT (forward if leaning back)
     → If motors push wrong way: motor direction or encoder sign wrong

  4. Check encoder sign:
       Roll robot forward by hand → /odom linear.x should be positive
     → If negative: negate encoder count in encoder.c

  5. Reduce KP to 10, KD to 0 and try again on test stand
       Increase KP slowly until robot starts fighting gravity

  6. Check /balance_state/fault_flags == 0
       Non-zero means safety_node or IMU fault is active

---

## NEW FILES TO CREATE IN PHASE 4

  scripts/record_session.sh
  ros2_ws/src/sbr_controller/src/pid_monitor_node.cpp
  docs/lessons/phase5-handoff-prompt.md

Update:
  scripts/check_topics.py              — add filtered odom + status topics
  ros2_ws/src/sbr_bringup/launch/teleop.launch.py
  ros2_ws/src/sbr_controller/CMakeLists.txt  — add pid_monitor_node
  ros2_ws/src/sbr_bringup/launch/robot.launch.py — add pid_monitor_node
  firmware/stm32/include/config/robot_params.h   — tuned gains
  README.md
  docs/lessons/yahboom-lesson-tracker.md

---

## CODING RULES (unchanged)

  All constants → robot_params.h. All pins → pin_config.h.
  No hardcoded IPs, credentials, or absolute paths.
  Commit format: "tune(stm32): ...", "fix(ros2): ...", "feat(ros2): ..."
  colcon test must stay green. pio test -e native must stay at ≥ 34 passes.

---

## READ FIRST

  firmware/stm32/CLAUDE.md
  docs/hardware/pin-map.md          ← wiring reference
  docs/software/pid-tuning.md       ← full tuning procedure
  docs/software/architecture.md     ← topic graph, UART protocol
  docs/lessons/yahboom-lesson-tracker.md

Then begin with Track B (B1–B5) if robot hardware is not yet ready,
or Track A if robot is assembled and Phase 3 hardware gate is passed.
