# Claude Code — Phase 2 Handoff Prompt

Copy everything below the line into Claude Code.

---

You are continuing the MicroROS Self-Balancing Robot project.
Read this entire prompt before taking any action.

---

## PHASE 1 COMPLETION SUMMARY (already done — do not redo)

All Phase 1 work is committed. Build is clean. Tests pass.

Files created in Phase 1:
  firmware/stm32/src/imu/icm20948.c        — ICM-20948 I2C3 driver
  firmware/stm32/src/imu/imu_filter.c      — complementary filter, cross-check
  firmware/stm32/src/motor/motor_driver.c  — TIM3/TIM4 PWM @ 20 kHz
  firmware/stm32/src/motor/encoder.c       — EXTI quadrature (not HW encoder mode)
  firmware/stm32/src/motor/odometry.c      — differential drive kinematics
  firmware/stm32/src/balance/balance_ctrl.c — cascade PID (vel→angle→motor)
  firmware/stm32/src/uart/uart_protocol.c  — binary framing, CRC-16/CCITT
  firmware/stm32/src/main.c               — 500Hz IMU ISR, 200Hz balance loop
  firmware/stm32/include/motor/motor_driver.h
  firmware/stm32/include/motor/encoder.h
  firmware/stm32/include/motor/odometry.h
  firmware/stm32/include/balance/balance_ctrl.h
  firmware/stm32/include/uart/uart_protocol.h
  firmware/stm32/test/native/test_all.c   — 34/34 tests passing
  firmware/stm32/Makefile

KEY DECISION made in Phase 1:
  PB7 conflict → resolved via Option A:
  ICM-20948 moved from I2C1 (PB6/PB7) to I2C3 (PA8=SCL, PC9=SDA)
  PB7 is now exclusively TIM4_CH2 (Motor 2 PWM).
  This is reflected in pin_config.h and docs/hardware/pin-map.md.

KEY CONSTRAINT discovered in Phase 1:
  Hardware TIM encoder mode is NOT available on PC4/PC5 and PB8/PB9
  on the STM32F429. Encoder is implemented via EXTI interrupts instead.
  Functionally identical. Do NOT attempt to switch to HW encoder mode.

Build state:
  pio run -e stm32f429 → SUCCESS (780B RAM, 19.7KB Flash)
  pio test -e native   → 34/34 PASS

---

## PHASE 2 OBJECTIVE

Implement the microROS communication layer on the STM32 so that the
Raspberry Pi 5 can see ROS2 topics published by the robot.

By the end of Phase 2:
  - /imu/data        publishes at ≥ 100 Hz on Pi 5
  - /odom            publishes at ≥ 50 Hz on Pi 5
  - /sonar/range     publishes at ≥ 10 Hz on Pi 5
  - /balance_state   publishes at ≥ 100 Hz on Pi 5
  - /cmd_vel         subscribed — drives balance_ctrl setpoint
  - microROS agent on Pi 5 sees all topics without errors
  - check_topics.py passes for all Phase 2 topics

Yahboom lesson ref: microROS basic §1–10, microROS board env §4–5

---

## SYSTEM ARCHITECTURE REMINDER

STM32 is the microROS CLIENT. It publishes sensor data and subscribes
to cmd_vel. It does NOT connect to WiFi.

ESP32 is a TRANSPARENT RELAY. It forwards UART bytes to UDP and back.
It does NOT parse or process microROS data.

Pi 5 runs the microROS AGENT (UDP port 8888). The agent bridges
microROS traffic to the full ROS2 DDS graph.

Data path:
  STM32 → uart_protocol frames → ESP32 → WiFi UDP → Pi 5 agent → ROS2

microROS on STM32 uses a CUSTOM TRANSPORT — it must use the existing
uart_protocol layer as its transport backend, not a direct UART write.
This means implementing the microROS custom transport callbacks:
  open_cb, close_cb, write_cb, read_cb

See: https://micro.ros.org/docs/tutorials/advanced/create_custom_transports/

---

## WHAT EXISTS (do not recreate)

  firmware/stm32/include/uros/uros_node.h     — API stubs (implement these)
  firmware/stm32/include/uros/publishers.h    — API stubs (implement these)
  firmware/stm32/include/uros/subscribers.h   — API stubs (implement these)
  firmware/stm32/src/uros/                    — EMPTY — your work goes here

  ros2_ws/src/sbr_interfaces/msg/BalanceState.msg  — message definition
  ros2_ws/src/sbr_interfaces/msg/RobotStatus.msg   — message definition
  ros2_ws/src/sbr_interfaces/srv/SetPidGains.srv   — service definition

  docker/docker-compose.yml  — agent service already defined
  scripts/check_topics.py    — validation script ready

---

## PHASE 2 STEPS (execute in order)

### Step 1 — Add microROS library to STM32 project

In firmware/stm32/platformio.ini, the lib_deps already references:
  https://github.com/micro-ROS/micro_ros_stm32cubemx_utils.git

Verify this builds. If the pre-built library does not include STM32F429,
you may need to build libmicroros.a from source:
  https://github.com/micro-ROS/micro_ros_stm32cubemx_utils

Flash is currently 19.7 KB. microROS static lib adds ~200-250 KB.
STM32F429ZIT6 has 2 MB Flash — plenty of headroom.

### Step 2 — Implement microROS custom transport (uart_protocol backend)

Create: firmware/stm32/src/uros/uros_transport.c
        firmware/stm32/include/uros/uros_transport.h

Implement the four micro-ROS custom transport callbacks using the
existing uart_protocol layer:

  bool transport_open(struct uxrCustomTransport * transport);
  bool transport_close(struct uxrCustomTransport * transport);
  size_t transport_write(struct uxrCustomTransport * transport,
                         const uint8_t * buf, size_t len, uint8_t * err);
  size_t transport_read(struct uxrCustomTransport * transport,
                        uint8_t * buf, size_t len, int timeout, uint8_t * err);

The write callback must wrap outgoing bytes in uart_protocol frames
(MSG_TYPE 0x01–0x04). The read callback must unwrap incoming frames
(MSG_TYPE 0x10–0x11) using the existing state-machine parser.

### Step 3 — Implement uros_node.c

Create: firmware/stm32/src/uros/uros_node.c

Implements functions declared in include/uros/uros_node.h:
  uros_node_init()   — init custom transport, create node, executor
  uros_node_spin()   — called from main loop (non-blocking, timeout=0)
  uros_node_ok()     — returns true while agent connected

Use FreeRTOS task for the executor spin if available, otherwise call
uros_node_spin() from the 100Hz tick in main.c.

Agent IP/port must come from a #define in robot_params.h or from NVS
(do not hardcode). Define UROS_AGENT_IP and UROS_AGENT_PORT in
robot_params.h — they will be overridden at build time in CI.

### Step 4 — Implement publishers.c

Create: firmware/stm32/src/uros/publishers.c

Publishes the following topics using data from existing modules:

  /imu/data      sensor_msgs/msg/Imu
                 Source: imu_filter_get_pitch() + icm20948_read()
                 Rate: 100 Hz (every 5th IMU sample at 500 Hz)
                 Frame: "imu_link"

  /odom          nav_msgs/msg/Odometry
                 Source: odometry module
                 Rate: 50 Hz
                 Frame: "odom" → "base_link"

  /sonar/range   sensor_msgs/msg/Range
                 Source: HC-SR04 measurement from main.c
                 Rate: 10 Hz
                 Frame: "base_link"
                 radiation_type: ULTRASOUND (1)
                 field_of_view: 0.261799 rad (15 degrees)
                 min_range: 0.02, max_range: 4.0

  /balance_state sbr_interfaces/msg/BalanceState
                 Source: balance_ctrl internals
                 Rate: 100 Hz
                 Fill all fields including fault_flags

### Step 5 — Implement subscribers.c

Create: firmware/stm32/src/uros/subscribers.c

Subscribes to:

  /cmd_vel   geometry_msgs/msg/Twist
             On receive:
               balance_ctrl_set_velocity(msg.linear.x)
               balance_ctrl_set_yaw_rate(msg.angular.z)
               Reset cmd_vel watchdog timer (500ms timeout in main.c)
             QoS: BEST_EFFORT, depth 1

  (SetPidGains service server — implement as a service, not subscriber)
  /set_pid_gains  sbr_interfaces/srv/SetPidGains
                  On call: pid_set_gains() for the specified loop_id
                  loop_id: 0=angle, 1=velocity, 2=steering, 3=speed

### Step 6 — Integrate into main.c

Update firmware/stm32/src/main.c to:
  1. Call uros_node_init() during startup (after all peripherals ready)
  2. Call uros_node_spin() in the 100 Hz tick (non-blocking)
  3. Call publisher functions at their respective rates:
       100 Hz tick → publish_imu(), publish_balance_state()
       50 Hz tick  → publish_odom()
       10 Hz tick  → publish_sonar()
  4. In the cmd_vel watchdog: if timeout fires, call
       balance_ctrl_set_velocity(0.0f)
       balance_ctrl_set_yaw_rate(0.0f)

### Step 7 — Start microROS agent on Pi 5

On Raspberry Pi 5:
  docker compose -f docker/docker-compose.yml up agent

Verify agent starts and listens on UDP 8888.

### Step 8 — Flash and validate end-to-end

Flash STM32:
  cd firmware/stm32 && pio run -e stm32f429 -t upload

On Pi 5, verify topics:
  python3 scripts/check_topics.py

Expected output: all Phase 2 topics PASS rate checks.

---

## BUILD CONSTRAINTS

Flash budget: STM32F429ZIT6 has 2048 KB Flash.
  Current usage: ~20 KB
  microROS lib:  ~220 KB (estimated)
  Remaining:     ~1800 KB — no concern.

RAM budget: 256 KB SRAM.
  Current usage: ~780 B
  microROS heap: ~20-40 KB (depends on publisher count)
  FreeRTOS:      ~8 KB
  Remaining:     ~200 KB — should be fine.

If RAM becomes tight: reduce UCLIENT_MAX_SESSION_CONNECTION_ATTEMPTS
and UCLIENT_UDP_TRANSPORT_MTU in microROS config.

---

## CODING RULES (same as Phase 1)

1. All physical constants and topic names → robot_params.h only.
2. All GPIO/timer assignments → pin_config.h only.
3. Never hardcode IP addresses or credentials.
4. No HAL calls in logic files (pid, filter, odometry) — these must
   remain x86-testable via pio test -e native.
5. Every new public function documented in its .h file.
6. Add unit tests for any new testable logic.
7. Commit after each step with message:
   "feat(stm32): <description>" or "feat(esp32): <description>"

---

## VALIDATION CHECKLIST (gate for Phase 3)

Run these after Step 8. All must pass before Phase 3 begins.

  [ ] pio run -e stm32f429  → build succeeds
  [ ] pio test -e native     → all tests pass (≥34)
  [ ] ros2 topic hz /imu/data         → ≥ 80 Hz
  [ ] ros2 topic hz /odom             → ≥ 40 Hz
  [ ] ros2 topic hz /sonar/range      → ≥ 8 Hz
  [ ] ros2 topic hz /balance_state    → ≥ 80 Hz
  [ ] ros2 service call /set_pid_gains → success response
  [ ] publish to /cmd_vel              → robot responds
  [ ] kill agent, wait 600ms           → robot stops (watchdog fires)
  [ ] python3 scripts/check_topics.py → all PASS

---

## FILES TO CREATE IN PHASE 2

  firmware/stm32/src/uros/uros_transport.c   — custom UART transport
  firmware/stm32/src/uros/uros_node.c        — node init + executor
  firmware/stm32/src/uros/publishers.c       — 4 topic publishers
  firmware/stm32/src/uros/subscribers.c      — cmd_vel sub + PID service
  firmware/stm32/include/uros/uros_transport.h

  Update:
  firmware/stm32/src/main.c                  — integrate uros_node
  firmware/stm32/include/config/robot_params.h — add UROS_* defines
  docs/lessons/yahboom-lesson-tracker.md     — mark lessons complete

---

## REFERENCE LINKS

microROS custom transport API:
  https://micro.ros.org/docs/tutorials/advanced/create_custom_transports/

microROS STM32 utilities:
  https://github.com/micro-ROS/micro_ros_stm32cubemx_utils

micro-ROS API reference:
  https://micro.ros.org/docs/api/

sensor_msgs/Imu message:
  https://docs.ros.org/en/humble/p/sensor_msgs/interfaces/msg/Imu.html

Yahboom microROS lesson pack:
  https://www.yahboom.net/study/SBR-microROS  (Track 2 §4-5, Track 3 §1-10)

---

Begin by reading:
  1. firmware/stm32/CLAUDE.md
  2. firmware/stm32/include/uros/uros_node.h
  3. firmware/stm32/include/uros/publishers.h
  4. firmware/stm32/include/uros/subscribers.h
  5. firmware/stm32/src/main.c   (understand the existing loop structure)
  6. docs/software/architecture.md  (UART frame protocol section)

Then proceed with Step 1.
