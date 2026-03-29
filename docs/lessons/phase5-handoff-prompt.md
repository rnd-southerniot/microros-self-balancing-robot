# Claude Code — Phase 5 Handoff Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
Read this entire prompt before taking any action.

---

## PROJECT STATE SUMMARY (Phases 1–4)

### What is complete

firmware/stm32/ — COMPLETE
  - ICM-20948 (I2C3: PA8/PC9), complementary filter, L3GD20 cross-check
  - TIM3/TIM4 PWM motors, EXTI quadrature encoder, odometry, cascade PID
  - DMA UART microROS transport, 4 publishers, cmd_vel sub, SetPidGains svc
  - Dual-mode: UROS_DISABLED (19.8KB) / UROS_ENABLED (with libmicroros.a)
  - 34/34 native tests passing

ros2_ws/src/ — COMPLETE
  - sbr_interfaces: BalanceState.msg, RobotStatus.msg, SetPidGains.srv
  - sbr_description: balancer.urdf.xacro (laser_frame placeholder present)
  - sbr_bringup: robot.launch.py, teleop.launch.py, ekf_params.yaml
  - sbr_controller: safety_node, telemetry_node, pid_monitor_node

docs/ — COMPLETE
  - pin-map.md, architecture.md, pid-tuning.md
  - record_session.sh (mcap format, 12 topics)
  - README.md updated with full quickstart + troubleshooting

### Phase 4 Track A status (hardware-dependent)

Track A (IMU calibration + PID tuning on physical robot) is the gate
for Phase 5 Track A hardware work. Verify the following are done before
starting Phase 5 hardware steps:

  [ ] IMU axes validated (accel_z ≈ 9.81 flat, pitch positive nose-up)
  [ ] Motor direction confirmed (positive cmd_vel = forward)
  [ ] IMU_ZERO_ANGLE_DEG calibrated and in robot_params.h
  [ ] Robot balances unsupported ≥ 30 seconds
  [ ] PID gains committed: "tune(stm32): PID gains validated on hardware"
  [ ] python3 scripts/check_topics.py → all PASS

### KEY CONSTRAINTS (never change)
  ICM-20948 → I2C3 (PA8=SCL, PC9=SDA). PB7 = TIM4_CH2 (Motor 2 PWM).
  Encoder → EXTI quadrature. No HW TIM encoder on PC4/PC5, PB8/PB9.
  microROS transport → raw XRCE-DDS over DMA UART.
  laser_frame TF already exists in balancer.urdf.xacro — do not refactor.

---

## PHASE 5 OBJECTIVE

Add LiDAR sensing, SLAM mapping, and Nav2 autonomous navigation.

This phase requires the Yahboom lidar kit hardware (2D TOF lidar,
typically YDLIDAR or similar, connected via USB or UART to Raspberry Pi 5).

By end of Phase 5:
  - Lidar scan publishing on /scan at ≥ 5 Hz
  - SLAM Toolbox building a map from lidar + odometry
  - Robot can be driven with teleop while building a map
  - Map saved and reloaded for localization
  - Nav2 navigating to a 2D pose goal autonomously
  - Robot avoids obstacles using lidar costmap
  - Ultrasonic HC-SR04 also active as a secondary safety layer
  - check_topics.py passes all checks including /scan + /map

Yahboom lesson ref:
  microROS basic §9 (lidar topics), ROS2 basic §21–23 (URDF/TF/Gazebo)

---

## PHASE 5 STEPS

### Step 1 — Identify and install lidar driver

Check which lidar model is included with your Yahboom kit.
Common options: YDLIDAR X4, X3, T-mini; SLAMTEC RPLIDAR A1.

Install the appropriate driver on Raspberry Pi 5:

  # YDLIDAR (most common with Yahboom kits)
  sudo apt install ros-jazzy-ydlidar-ros2-driver

  # RPLIDAR
  sudo apt install ros-jazzy-rplidar-ros

Check USB device:
  ls /dev/ttyUSB* or ls /dev/ttyACM*

Set permissions:
  sudo usermod -aG dialout $USER   # log out and back in

Verify lidar spins and publishes:
  ros2 launch ydlidar_ros2_driver ydlidar_launch.py
  ros2 topic hz /scan   # expect ≥ 5 Hz
  ros2 topic echo /scan --once

### Step 2 — Verify laser_frame TF

The laser_frame is already defined in balancer.urdf.xacro as a fixed
joint at xyz="0.080 0 0.120" (front, 12cm above base_link).

Verify the transform is being broadcast after robot.launch.py starts:
  ros2 run tf2_tools view_frames
  # laser_frame must appear as child of base_link

Check the lidar driver's frame_id matches:
  ros2 topic echo /scan --field header.frame_id
  # Must be "laser_frame" — configure in driver launch params if not

If the physical lidar is mounted at a different position or orientation,
update the laser_joint origin in balancer.urdf.xacro accordingly.
Document the actual mounting position in docs/hardware/pin-map.md.

### Step 3 — Add lidar to robot.launch.py

Update ros2_ws/src/sbr_bringup/launch/robot.launch.py to include
the lidar driver node. Add a launch argument for lidar model:

  lidar_model_arg = DeclareLaunchArgument(
      'lidar_model',
      default_value='ydlidar_x4',
      description='Lidar driver type: ydlidar_x4, rplidar_a1'
  )

Create ros2_ws/src/sbr_bringup/config/lidar_params.yaml:
  - frame_id: laser_frame
  - port: /dev/ttyUSB0
  - baudrate: 128000 (YDLIDAR X4) or 115200 (RPLIDAR A1)
  - angle_min, angle_max, range_min, range_max per your model

Create ros2_ws/src/sbr_bringup/launch/lidar.launch.py as a standalone
lidar-only launch for testing without the full stack.

### Step 4 — Install SLAM Toolbox

  sudo apt install ros-jazzy-slam-toolbox

Add slam_toolbox_params.yaml to ros2_ws/src/sbr_bringup/config/:

  slam_toolbox:
    ros__parameters:
      solver_plugin: solver_plugins::CeresSolver
      ceres_linear_solver: SPARSE_NORMAL_CHOLESKY
      ceres_preconditioner: SCHUR_JACOBI
      odom_frame: odom
      map_frame: map
      base_frame: base_link
      scan_topic: /scan
      use_scan_matching: true
      use_scan_barycenter: true
      minimum_travel_distance: 0.2
      minimum_travel_heading: 0.3
      map_update_interval: 5.0
      resolution: 0.05
      max_laser_range: 8.0
      mode: mapping              # 'mapping' or 'localization'

Create ros2_ws/src/sbr_bringup/launch/slam.launch.py:
  - Starts SLAM Toolbox in async mapping mode
  - Use /odometry/filtered (EKF output) as odometry source, not raw /odom

### Step 5 — First mapping session

Full stack launch sequence:

  Terminal 1:
    docker compose -f docker/docker-compose.yml up agent

  Terminal 2:
    ros2 launch sbr_bringup robot.launch.py
    ros2 launch sbr_bringup lidar.launch.py

  Terminal 3:
    ros2 launch sbr_bringup slam.launch.py

  Terminal 4 (from Host VM with RVIZ2):
    ros2 launch nav2_bringup rviz_launch.py
    # Add displays: Map, LaserScan, RobotModel, TF

Drive the robot around with teleop:
    ros2 launch sbr_bringup teleop.launch.py

Verify in RVIZ2:
  - Lidar scan visible around robot
  - Map building incrementally as robot moves
  - TF tree: map → odom → base_link → laser_frame (all connected)

If map is distorted or rotated: check laser_frame TF orientation.
If map has gaps: lidar range_max may be set too low.

### Step 6 — Save map

  ros2 run nav2_map_server map_saver_cli -f maps/my_first_map

This creates maps/my_first_map.yaml and maps/my_first_map.pgm.
Add maps/*.pgm to .gitignore (binary). Commit maps/*.yaml (metadata only).

Create maps/ directory and add maps/*.pgm to .gitignore.

### Step 7 — Install Nav2 and configure

  sudo apt install ros-jazzy-nav2-bringup

Create ros2_ws/src/sbr_bringup/config/nav2_params.yaml.
Key parameters for a differential-drive self-balancing robot:

  controller_server:
    ros__parameters:
      controller_frequency: 20.0
      FollowPath:
        plugin: "nav2_regulated_pure_pursuit_controller::RegulatedPurePursuitController"
        desired_linear_vel: 0.3
        max_linear_decel_ratio: 1.0
        use_velocity_scaled_lookahead_dist: false
        min_lookahead_dist: 0.3
        max_lookahead_dist: 0.9

  local_costmap:
    local_costmap:
      ros__parameters:
        update_frequency: 10.0
        publish_frequency: 5.0
        width: 3
        height: 3
        resolution: 0.05
        robot_radius: 0.15

  global_costmap:
    global_costmap:
      ros__parameters:
        update_frequency: 1.0
        robot_radius: 0.15

  behavior_server:
    ros__parameters:
      local_costmap_topic: local_costmap/costmap_raw
      local_footprint_topic: local_costmap/published_footprint

IMPORTANT: Nav2 publishes to /cmd_vel directly by default. Remap it
to /cmd_vel_nav so twist_mux arbitrates priority correctly:
  In nav2_params.yaml, set FollowPath.cmd_vel_topic: /cmd_vel_nav
  OR remap in the Nav2 launch node arguments.

### Step 8 — Localization with saved map

Update slam_toolbox_params.yaml:
  mode: localization
  map_file_name: <absolute path to maps/my_first_map.yaml>

Or create a separate localization launch file:
  ros2_ws/src/sbr_bringup/launch/localization.launch.py

Start full navigation stack:
  ros2 launch sbr_bringup robot.launch.py
  ros2 launch sbr_bringup localization.launch.py
  ros2 launch nav2_bringup navigation_launch.py \
    params_file:=ros2_ws/src/sbr_bringup/config/nav2_params.yaml

Set initial pose in RVIZ2 using "2D Pose Estimate" button.
Send goal using "Nav2 Goal" button.
Verify: robot navigates to goal without falling.

### Step 9 — Ultrasonic integration as Nav2 layer

The HC-SR04 /sonar/range topic (10 Hz) can feed a Nav2 range sensor
costmap layer for close-range obstacle avoidance:

In nav2_params.yaml local_costmap:
  plugins: ["obstacle_layer", "inflation_layer", "range_sensor_layer"]
  range_sensor_layer:
    plugin: nav2_costmap_2d::RangeSensorLayer
    topics: ["/sonar/range"]
    phi: 1.2
    inflate_cone: 1.0
    no_readings_timeout: 2.0
    clear_threshold: 0.2
    mark_threshold: 0.8

### Step 10 — Update check_topics.py and lesson tracker

Add to scripts/check_topics.py:
  ("/scan",  4,  30),
  ("/map",   0.1, 2),

Update docs/lessons/yahboom-lesson-tracker.md — mark:
  Track 3 §9 (lidar publish) ✅
  Track 4 §22 (Gazebo — deferred) ⏭️
  Track 4 §23 (TF2) ✅

Write docs/lessons/phase6-handoff-prompt.md for vision module.

---

## VALIDATION GATE — Phase 5 complete when ALL pass

Hardware:
  [ ] /scan publishes at ≥ 5 Hz with valid data
  [ ] laser_frame TF connected to base_link correctly
  [ ] Map builds correctly while driving around a room

Software:
  [ ] colcon build → 0 errors
  [ ] Map saved and reloaded successfully
  [ ] Nav2 navigates to a 2D goal without falling
  [ ] Obstacle avoidance: robot stops when object placed in path
  [ ] python3 scripts/check_topics.py → all PASS (including /scan)

---

## LIKELY ISSUES AND FIXES

| Issue | Cause | Fix |
|-------|-------|-----|
| /scan not publishing | Wrong USB port or permissions | ls /dev/ttyUSB*, sudo usermod -aG dialout |
| laser_frame not in TF tree | robot_state_publisher not running | Verify robot.launch.py started |
| Map rotates or drifts | laser_frame orientation wrong | Check rpy in laser_joint in URDF |
| Nav2 drives in circles | cmd_vel not remapped from Nav2 | Remap /cmd_vel → /cmd_vel_nav in nav2_params |
| Robot falls during navigation | Nav2 commands too high linear vel | Reduce desired_linear_vel to 0.2 |
| SLAM map has holes | range_max too small | Increase max_laser_range in slam_params |
| Costmap not clearing | Lidar scan rate too low | Ensure /scan ≥ 5 Hz |
| EKF diverges with lidar | odom noise too high | Tune ekf_params.yaml covariances |

---

## NEW FILES TO CREATE IN PHASE 5

  ros2_ws/src/sbr_bringup/launch/lidar.launch.py
  ros2_ws/src/sbr_bringup/launch/slam.launch.py
  ros2_ws/src/sbr_bringup/launch/localization.launch.py
  ros2_ws/src/sbr_bringup/config/lidar_params.yaml
  ros2_ws/src/sbr_bringup/config/slam_toolbox_params.yaml
  ros2_ws/src/sbr_bringup/config/nav2_params.yaml
  maps/                   (directory — add *.pgm to .gitignore)
  docs/lessons/phase6-handoff-prompt.md

Update:
  ros2_ws/src/sbr_bringup/launch/robot.launch.py  — add lidar arg
  ros2_ws/src/sbr_description/urdf/balancer.urdf.xacro  — verify laser_joint
  scripts/check_topics.py  — add /scan and /map
  docs/lessons/yahboom-lesson-tracker.md

---

## CODING RULES (unchanged)

  All constants → robot_params.h. All pins → pin_config.h.
  No hardcoded device paths — use launch arguments with defaults.
  Commit: "feat(ros2): ..." / "fix(slam): ..." / "tune(nav2): ..."
  colcon test + pio test -e native must stay green.

---

## READ FIRST

  docs/lessons/phase5-handoff-prompt.md   ← this file
  ros2_ws/src/sbr_description/urdf/balancer.urdf.xacro  ← laser_frame location
  ros2_ws/src/sbr_bringup/launch/robot.launch.py
  docs/software/architecture.md
  docs/lessons/yahboom-lesson-tracker.md

Then proceed with Step 1 — identify your lidar model.
