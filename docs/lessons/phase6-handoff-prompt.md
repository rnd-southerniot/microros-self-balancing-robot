# Claude Code — Phase 6 Handoff Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
Read this entire prompt before taking any action.

---

## PROJECT STATE SUMMARY (Phases 1–5 complete)

### What is complete and committed

firmware/stm32/ — COMPLETE (34/34 native tests, UROS_ENABLED/DISABLED)
  - ICM-20948 (I2C3 PA8/PC9), EXTI encoder, cascade PID, DMA UART transport
  - 4 microROS publishers, cmd_vel sub, SetPidGains service

ros2_ws/src/ — COMPLETE (colcon build clean)
  - sbr_interfaces, sbr_description (laser_frame in URDF), sbr_bringup
  - sbr_controller: safety_node, telemetry_node, pid_monitor_node
  - Launch files: robot.launch.py, teleop.launch.py, lidar.launch.py,
    slam.launch.py, localization.launch.py, nav2.launch.py
  - Configs: lidar_params.yaml, slam_toolbox_params.yaml, nav2_params.yaml,
    ekf_params.yaml, twist_mux.yaml

### CRITICAL SAFETY DESIGN — do not change
  Nav2 → /cmd_vel_nav (not /cmd_vel). twist_mux arbitrates.
  teleop priority 20 > nav2 priority 10. safety_node overrides all.
  This prevents Nav2 from sending full-speed commands that topple the robot.

### KEY CONSTRAINTS — do not change
  ICM-20948 → I2C3 (PA8/PC9). PB7 = TIM4_CH2 only.
  Encoder → EXTI quadrature.
  microROS transport → raw XRCE-DDS over DMA UART.

### Hardware status
  Phase 4 Track A (PID tuning) — pending physical robot bring-up
  Phase 5 hardware (LiDAR + SLAM + Nav2) — pending lidar kit delivery
  Phase 6 (vision) — pending CSI/USB camera attachment to Pi 5

---

## PHASE 6 OBJECTIVE

Add computer vision capabilities using the Raspberry Pi 5's CSI camera
or a USB camera. The Pi 5 has sufficient compute for lightweight
inference without a Jetson or GPU.

Target capabilities (implement in order of complexity):
  1. Camera stream → /camera/image_raw topic (foundation for all vision)
  2. QR code recognition → publishes decoded QR content
  3. Face tracking → publishes face bounding box, drives /cmd_vel_teleop
  4. Human posture recognition → publishes skeleton keypoints
  5. Gesture control → maps gestures to cmd_vel commands

Yahboom lesson ref: ESP32 §17 (WiFi camera), vision module lessons

By end of Phase 6:
  - /camera/image_raw publishing at ≥ 15 Hz
  - /camera/camera_info publishing (calibration)
  - QR code node: /qr_code/data (std_msgs/String) on detection
  - Face tracking node: /face/bbox, drives robot to keep face centred
  - All vision nodes launch-file managed, individually enable/disable-able
  - check_topics.py updated with /camera/image_raw

---

## PHASE 6 STEPS

### Step 1 — Camera hardware setup

Raspberry Pi 5 supports:
  Option A: CSI camera (Raspberry Pi Camera Module v2 or v3)
    - Connects to MIPI CSI connector on Pi 5
    - Best performance, lowest latency
    - Driver: libcamera / picamera2

  Option B: USB camera (any UVC-compatible webcam)
    - Plug in to USB port
    - Driver: v4l2
    - More flexible mounting

Detect camera:
  # CSI
  libcamera-hello --list-cameras

  # USB
  ls /dev/video*
  v4l2-ctl --list-devices

Install dependencies:
  sudo apt install ros-jazzy-image-pipeline \
                   ros-jazzy-camera-calibration \
                   ros-jazzy-vision-opencv \
                   python3-opencv \
                   python3-picamera2    # CSI only

### Step 2 — Camera ROS2 node

Create ros2_ws/src/sbr_vision/ as a new package.

For CSI camera (recommended on Pi 5):
  Package: ros2_ws/src/sbr_vision/
  Node:    camera_node.py using picamera2 + cv_bridge
  Publishes:
    /camera/image_raw      sensor_msgs/Image      @ 30 Hz
    /camera/camera_info    sensor_msgs/CameraInfo @ 30 Hz

For USB camera:
  Use ros-jazzy-usb-cam package:
    sudo apt install ros-jazzy-usb-cam
    Configure in config/camera_params.yaml

Create ros2_ws/src/sbr_bringup/launch/vision.launch.py:
  - Starts camera node
  - Optionally starts QR / face / gesture nodes
  - Launch arguments: enable_qr:=true enable_face:=false etc.

Create ros2_ws/src/sbr_bringup/config/camera_params.yaml:
  camera_node:
    ros__parameters:
      width: 640
      height: 480
      fps: 30
      frame_id: camera_frame

### Step 3 — Camera calibration

Calibrate intrinsics using the ROS2 camera calibration tool:
  ros2 run camera_calibration cameracalibrator \
    --size 8x6 --square 0.025 \
    image:=/camera/image_raw \
    camera:=/camera

Move output calibration file to:
  ros2_ws/src/sbr_bringup/config/camera_calibration.yaml

Add camera_frame TF to balancer.urdf.xacro (already has placeholder joint).
Update camera_joint xyz/rpy to match physical mounting.

### Step 4 — QR code recognition node

Create ros2_ws/src/sbr_vision/sbr_vision/qr_node.py

Dependencies:
  pip install pyzbar
  sudo apt install libzbar0

Node behaviour:
  - Subscribes /camera/image_raw
  - Runs pyzbar decode on every frame (or every 3rd for CPU savings)
  - On detection: publishes /qr_code/data (std_msgs/String)
    and /qr_code/image (sensor_msgs/Image) with bounding box overlay
  - Throttle: only publish when new QR content detected (not every frame)

Add to sbr_interfaces:
  msg/QRDetection.msg:
    std_msgs/Header header
    string data          # decoded QR string
    int32 x, y, w, h    # bounding box pixels

### Step 5 — Face tracking node

Create ros2_ws/src/sbr_vision/sbr_vision/face_track_node.py

Dependencies:
  pip install opencv-python mediapipe

Node behaviour:
  - Subscribes /camera/image_raw
  - Detects faces using OpenCV Haar cascade or MediaPipe Face Detection
  - Publishes /face/detections (sbr_interfaces/FaceDetections)
  - Publishes /face/image with overlay
  - When tracking enabled: computes face centre error from image centre
    and publishes proportional /cmd_vel_teleop.angular.z to keep face
    centred. P-controller: angular.z = -Kp * (face_x - image_width/2)

Add to sbr_interfaces:
  msg/FaceDetection.msg:
    std_msgs/Header header
    int32 count          # number of faces detected
    float32 face_x       # normalised x of primary face centre [0..1]
    float32 face_y       # normalised y of primary face centre [0..1]
    float32 face_w       # normalised width of primary face
    bool tracking_active

Launch argument: enable_face_tracking:=false (off by default for safety).
When face tracking active: set linear.x = 0 (face tracking only steers).

### Step 6 — Gesture control node (optional / advanced)

Create ros2_ws/src/sbr_vision/sbr_vision/gesture_node.py

Dependencies:
  pip install mediapipe

Use MediaPipe Hands to detect hand landmarks.
Map gestures to cmd_vel commands:

  Gesture           → cmd_vel
  Open palm (stop)  → linear.x=0, angular.z=0
  Thumbs up         → linear.x=+0.3 (forward)
  Thumbs down       → linear.x=-0.3 (backward)
  Point left        → angular.z=+0.5
  Point right       → angular.z=-0.5

Publish gesture commands to /cmd_vel_teleop (same as keyboard teleop).
twist_mux priority 20 ensures gesture control can override Nav2.

Add to sbr_interfaces:
  msg/GestureCommand.msg:
    std_msgs/Header header
    string gesture_name
    float32 confidence

### Step 7 — Package structure and launch integration

ros2_ws/src/sbr_vision/
  package.xml
  CMakeLists.txt        (or setup.py for pure Python)
  sbr_vision/
    __init__.py
    camera_node.py
    qr_node.py
    face_track_node.py
    gesture_node.py     (optional)
  launch/
    vision.launch.py
  config/
    camera_params.yaml

Add sbr_vision as optional include in robot.launch.py:
  enable_vision_arg = DeclareLaunchArgument('enable_vision', default_value='false')
  If enable_vision=true: IncludeLaunchDescription(vision.launch.py)

### Step 8 — CPU performance on Pi 5

Pi 5 has 4× Cortex-A76 cores. Assign vision nodes to specific cores:
  In launch file, set cpu_affinity if needed.
  MediaPipe runs efficiently on ARM64 — test inference time with:
    time ros2 run sbr_vision face_track_node

Monitor CPU during full stack:
  htop   # all cores
  vcgencmd measure_temp   # keep below 80°C

If CPU too high:
  - Reduce camera resolution to 320×240
  - Process every 2nd or 3rd frame
  - Disable gesture node when not needed

### Step 9 — Update check_topics.py and lesson tracker

Add to scripts/check_topics.py:
  ("/camera/image_raw", 10, 35),

Update docs/lessons/yahboom-lesson-tracker.md:
  Track 1 §17 (WiFi camera) — mark ✅ (CSI/USB replaces WiFi cam approach)

Update README.md with vision quickstart:
  ros2 launch sbr_bringup vision.launch.py enable_qr:=true

---

## PACKAGE STRUCTURE FOR sbr_vision

  ros2_ws/src/sbr_vision/
  ├── package.xml
  ├── setup.py              (Python package — use ament_python)
  ├── setup.cfg
  ├── resource/sbr_vision
  ├── sbr_vision/
  │   ├── __init__.py
  │   ├── camera_node.py
  │   ├── qr_node.py
  │   └── face_track_node.py
  └── launch/
      └── vision.launch.py

package.xml dependencies:
  rclpy, sensor_msgs, std_msgs, cv_bridge, sbr_interfaces

CMakeLists.txt (ament_python):
  find_package(ament_cmake_python REQUIRED)
  ament_python_install_package(${PROJECT_NAME})
  install(PROGRAMS sbr_vision/camera_node.py DESTINATION lib/${PROJECT_NAME})

### New messages in sbr_interfaces

Add to ros2_ws/src/sbr_interfaces/:
  msg/QRDetection.msg
  msg/FaceDetection.msg
  msg/GestureCommand.msg

Re-run colcon build after adding new messages:
  colcon build --packages-select sbr_interfaces
  source install/setup.bash
  colcon build --symlink-install

---

## VALIDATION GATE — Phase 6 complete when ALL pass

  [ ] /camera/image_raw publishes ≥ 15 Hz
  [ ] /camera/camera_info matches calibration
  [ ] camera_frame TF connected in URDF
  [ ] QR code node detects and publishes /qr_code/data
  [ ] Face tracking node publishes /face/detections
  [ ] Face tracking drives angular.z proportionally when enabled
  [ ] CPU temperature stays < 80°C with full stack running
  [ ] python3 scripts/check_topics.py — all PASS
  [ ] colcon build + colcon test — 0 errors

---

## CODING RULES (unchanged)

  All physical constants → robot_params.h. All pins → pin_config.h.
  No hardcoded device paths — launch args with defaults.
  No hardcoded IPs or credentials.
  Commit: "feat(vision): ..." / "fix(vision): ..."
  colcon test + pio test -e native stay green throughout.
  Vision nodes must degrade gracefully if camera not connected
  (log warning, don't crash the whole stack).

---

## READ FIRST

  docs/lessons/phase6-handoff-prompt.md   ← this file
  ros2_ws/src/sbr_description/urdf/balancer.urdf.xacro  ← camera_frame joint
  ros2_ws/src/sbr_bringup/launch/robot.launch.py
  ros2_ws/src/sbr_interfaces/  ← add new msg files here
  docs/lessons/yahboom-lesson-tracker.md

Then proceed with Step 1 — identify camera hardware.
