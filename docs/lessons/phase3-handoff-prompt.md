# Claude Code — Phase 3 Handoff Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
Read this entire prompt before taking any action.

---

## PHASE 1 + 2 COMPLETION SUMMARY (already done — do not redo)

### Phase 1 — STM32 firmware core (COMPLETE)
  src/imu/icm20948.c         — ICM-20948 on I2C3 (PA8/PC9)
  src/imu/imu_filter.c       — complementary filter, L3GD20 cross-check
  src/motor/motor_driver.c   — TIM3/TIM4 PWM @ 20 kHz
  src/motor/encoder.c        — EXTI quadrature (NOT hardware encoder mode)
  src/motor/odometry.c       — differential drive kinematics
  src/balance/balance_ctrl.c — cascade PID (vel → angle → motor + steering)
  src/uart/uart_protocol.c   — binary framing, CRC-16/CCITT
  src/main.c                 — 500Hz IMU ISR, 200Hz balance loop

### Phase 2 — microROS communication layer (COMPLETE)
  src/uros/uros_transport.c  — DMA circular UART transport (USART1, 512B buf)
  src/uros/uros_node.c       — node init, executor, ping/reconnect
  src/uros/publishers.c      — /imu/data /odom /sonar/range /balance_state
  src/uros/subscribers.c     — /cmd_vel subscriber + SetPidGains service
  scripts/build_microros_lib.sh — Docker build for microROS static library
  scripts/link_microros.py   — PlatformIO auto-detect: UROS_ENABLED/DISABLED

### KEY DECISIONS (do not change)
  PB7 conflict → Option A: ICM-20948 on I2C3 (PA8=SCL, PC9=SDA)
  Encoder → EXTI quadrature (hardware TIM encoder mode unavailable on these pins)
  microROS transport → raw XRCE-DDS bytes over DMA UART (no CRC framing)
  Build mode → dual: UROS_DISABLED (no lib, 19.8KB Flash) / UROS_ENABLED (with lib)

### Build state
  pio run -e stm32f429  → SUCCESS (780B RAM, 19.8KB Flash — UROS_DISABLED)
  pio test -e native    → 34/34 PASS

---

## PHASE 3 OBJECTIVE

Activate microROS end-to-end on real hardware, bring up the full
ROS2 stack on Raspberry Pi 5, and validate that all topics are live.

Then implement the ROS2 workspace packages so the robot can be
controlled from the Pi 5 with keyboard teleop.

By end of Phase 3:
  - libmicroros.a built and linked — UROS_ENABLED active
  - STM32 publishes all 4 topics live to Pi 5
  - ROS2 workspace builds cleanly on Pi 5 (colcon build)
  - safety_node and telemetry_node running
  - robot_localization EKF fusing /imu/data + /odom
  - Keyboard teleop drives the robot while balancing
  - check_topics.py passes all checks
  - robot responds to cmd_vel and stops within 500ms when teleop killed

Yahboom lesson ref:
  microROS board env §5 (agent), ROS2 basic §1–8, §11, §16–17, §19

---

## PHASE 3 STEPS (execute in order)

### Step 1 — Build microROS static library on Pi 5

On Raspberry Pi 5 (requires Docker):

  cd firmware/stm32
  bash scripts/build_microros_lib.sh

This script:
  - Pulls micro-ROS/micro_ros_stm32cubemx_utils Docker image
  - Builds libmicroros.a for STM32F429 + FreeRTOS
  - Includes sbr_interfaces custom messages (BalanceState, RobotStatus)
  - Outputs to lib/microros/libmicroros.a

Expected build time: 5–15 minutes on first run.

Verify the library exists:
  ls -lh lib/microros/libmicroros.a

Then rebuild firmware with microROS enabled:
  pio run -e stm32f429

link_microros.py detects the library and defines UROS_ENABLED automatically.
Expected Flash: ~240 KB. RAM: ~45 KB. Both well within STM32F429 budget.

If library build fails, check lib/microros/README.md for troubleshooting.

### Step 2 — Flash STM32 and verify UART output

  pio run -e stm32f429 -t upload

Connect serial monitor on USART1 (PA9/PA10) @ 115200 to confirm boot:
  pio device monitor

Expected boot log:
  [sbr] IMU OK
  [sbr] Motors OK
  [sbr] uROS transport ready
  [sbr] Waiting for agent...

If "Waiting for agent..." loops indefinitely, the agent is not yet running.
That is expected — continue to Step 3.

### Step 3 — Start microROS agent on Pi 5

  docker compose -f docker/docker-compose.yml up agent

Verify in docker logs: "Listening on UDP port 8888"

On first connection from STM32, the agent log should show:
  "New client connected. Client ID: 1"

### Step 4 — Validate all microROS topics

On Pi 5 (with agent running and STM32 connected):

  source /opt/ros/humble/setup.bash

  # Check all expected topics appear
  ros2 topic list

  # Check rates
  ros2 topic hz /imu/data         # expect ≥ 100 Hz
  ros2 topic hz /odom             # expect ≥ 50 Hz
  ros2 topic hz /balance_state    # expect ≥ 100 Hz
  ros2 topic hz /sonar/range      # expect ≥ 10 Hz

  # Check message content
  ros2 topic echo /imu/data --once
  ros2 topic echo /balance_state --once

  # Full validation
  python3 scripts/check_topics.py

All checks must pass before Step 5.

If /sonar/range shows NaN: expected — HC-SR04 not wired yet.
If /imu/data shows zero quaternion: check ICM-20948 I2C3 wiring (PA8/PC9).

### Step 5 — Build ROS2 workspace on Pi 5

Run setup script if not already done:
  bash scripts/setup_pi5.sh

Then build:
  cd ros2_ws
  source /opt/ros/humble/setup.bash
  rosdep install --from-paths src --ignore-src -r -y
  colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release

Build order is automatically resolved by colcon:
  sbr_interfaces → sbr_description → sbr_controller → sbr_bringup

Expected result: 4 packages built, 0 warnings, 0 errors.

If sbr_interfaces fails: it must build first. Run:
  colcon build --packages-select sbr_interfaces
  source install/setup.bash
  colcon build --symlink-install

### Step 6 — Fix any ROS2 package issues

After Step 5 colcon build, run tests:
  colcon test --event-handlers console_cohesion+
  colcon test-result --verbose

Fix any ament_cpplint or ament_flake8 failures before proceeding.

Common issues to check:
  - safety_node.cpp: tf2 quaternion extraction from IMU message
  - telemetry_node.cpp: sbr_interfaces dependency linked correctly
  - robot.launch.py: URDF xacro path resolution on Pi 5

### Step 7 — Launch full robot stack

  source /opt/ros/humble/setup.bash
  source ros2_ws/install/setup.bash
  ros2 launch sbr_bringup robot.launch.py

Expected nodes to start (check with ros2 node list):
  /micro_ros_agent
  /robot_state_publisher
  /ekf_filter_node
  /safety_node
  /telemetry_node
  /twist_mux

Verify EKF is fusing correctly:
  ros2 topic hz /odometry/filtered    # expect ≥ 40 Hz
  ros2 topic echo /odometry/filtered --once

### Step 8 — Keyboard teleop

In a second terminal on Pi 5:
  source /opt/ros/humble/setup.bash
  source ros2_ws/install/setup.bash
  ros2 run teleop_twist_keyboard teleop_twist_keyboard \
    --ros-args --remap cmd_vel:=/cmd_vel_teleop

Verify:
  - ros2 topic echo /cmd_vel shows values when keys pressed
  - twist_mux passes through to /cmd_vel
  - STM32 /balance_state shows velocity_setpoint changing
  - Robot responds (if physically assembled and on test stand)

Watchdog test:
  Kill teleop (Ctrl+C) → within 500ms, /cmd_vel should show zero
  /balance_state/fault_flags bit 4 (cmd_vel timeout) should briefly set

### Step 9 — SetPidGains service test

  ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
    "{loop_id: 0, kp: 25.0, ki: 0.0, kd: 0.8}"

Expected response: success: true, message: "OK", gains echoed back.

Then verify balance_state reflects the new gains in the next publish cycle.

### Step 10 — Run full validation

  python3 scripts/check_topics.py

All checks must pass. If any fail, fix before marking Phase 3 complete.

---

## LIKELY ISSUES AND FIXES

| Issue | Cause | Fix |
|-------|-------|-----|
| libmicroros.a build fails | Docker not installed | bash scripts/setup_pi5.sh first |
| "No module named sbr_interfaces" | Build order wrong | colcon build --packages-select sbr_interfaces first |
| /imu/data quaternion all zeros | ICM-20948 I2C3 not initialized | Check I2C3 HAL init in main.c; verify PA8/PC9 wiring |
| EKF not publishing /odometry/filtered | ekf_params.yaml path wrong | Check share/${PROJECT_NAME}/config install path |
| twist_mux not routing cmd_vel | Remap wrong in launch file | Verify cmd_vel_teleop → twist_mux input topic name |
| safety_node kills cmd_vel immediately | IMU quaternion is (0,0,0,1) = 0° pitch | Correct, should not trigger. Check tilt threshold |
| colcon build sbr_controller fails | tf2/tf2_ros missing | sudo apt install ros-humble-tf2-ros |
| microROS agent loses connection | WiFi packet loss | Use 5GHz WiFi; check ESP32 UDP watchdog |

---

## PERFORMANCE TARGETS FOR PHASE 3

After Step 7 (full stack running):

| Metric | Target | Measure with |
|--------|--------|-------------|
| /imu/data rate | ≥ 100 Hz | ros2 topic hz |
| /odom rate | ≥ 50 Hz | ros2 topic hz |
| /odometry/filtered rate | ≥ 40 Hz | ros2 topic hz |
| /balance_state rate | ≥ 100 Hz | ros2 topic hz |
| WiFi round-trip | < 20 ms | ping Pi5 from ESP32 serial log |
| cmd_vel watchdog | < 600 ms | kill teleop, watch /balance_state |
| colcon build time | < 3 min | time colcon build |

---

## FILES TO CREATE / MODIFY IN PHASE 3

  Modify firmware/stm32 — only if needed to fix UROS_ENABLED issues
  No new firmware files expected unless bugs found in Phase 2

  Possibly fix:
    ros2_ws/src/sbr_bringup/launch/robot.launch.py   — URDF path, remaps
    ros2_ws/src/sbr_bringup/config/ekf_params.yaml   — tune covariances
    ros2_ws/src/sbr_controller/src/safety_node.cpp   — tf2 import fix
    ros2_ws/src/sbr_controller/src/telemetry_node.cpp — compilation fix
    ros2_ws/src/sbr_controller/CMakeLists.txt         — dependency links

  Create:
    ros2_ws/src/sbr_bringup/launch/teleop.launch.py  — teleop + twist_mux
    docs/software/ros2-setup.md                      — Pi 5 setup notes
    docs/lessons/phase4-handoff-prompt.md            — after Phase 3 done

  Update:
    docs/lessons/yahboom-lesson-tracker.md           — mark completed
    scripts/check_topics.py                          — add /odometry/filtered

---

## CODING RULES (unchanged from Phase 1+2)

1. All constants → robot_params.h. All pins → pin_config.h.
2. No HAL calls in testable modules (pid, filter, odometry).
3. No hardcoded IPs, credentials, or paths.
4. Commit format: "feat(ros2): <description>" or "fix(ros2): <description>"
5. Every fix in ros2_ws must keep colcon test green.

---

## VALIDATION GATE (all required before Phase 4)

  [ ] libmicroros.a built, UROS_ENABLED active, firmware flashed
  [ ] ros2 topic hz /imu/data        → ≥ 80 Hz
  [ ] ros2 topic hz /odom            → ≥ 40 Hz
  [ ] ros2 topic hz /balance_state   → ≥ 80 Hz
  [ ] ros2 topic hz /odometry/filtered → ≥ 40 Hz
  [ ] colcon build → 0 errors, 0 warnings
  [ ] colcon test  → all pass
  [ ] ros2 launch sbr_bringup robot.launch.py → all nodes start
  [ ] teleop drives /cmd_vel → balance_state velocity_setpoint changes
  [ ] SetPidGains service → success response
  [ ] cmd_vel watchdog → stops within 600ms
  [ ] python3 scripts/check_topics.py → all PASS

---

## READ FIRST

  firmware/stm32/CLAUDE.md
  firmware/stm32/lib/microros/README.md       ← library build instructions
  firmware/stm32/scripts/build_microros_lib.sh
  ros2_ws/src/sbr_bringup/launch/robot.launch.py
  ros2_ws/src/sbr_controller/src/safety_node.cpp
  docs/software/architecture.md
  docs/lessons/yahboom-lesson-tracker.md

Then proceed with Step 1.
