# PID Tuning Guide

## Overview

The balance controller uses three cascaded PID loops:

```
cmd_vel.linear.x
    │
    ▼
[Velocity PID] ──► pitch setpoint offset
                         │
                         ▼
                  [Angle PID] ──► motor torque
                         │
    ┌────────────────────┘
    │
    ▼
[Steering PID] ──► differential torque
```

Tune in order: **Angle → Velocity → Steering**. Never tune all three simultaneously.

---

## Phase 1: Angle (Inner) Loop

**Goal:** Robot holds vertical ±2° for 10+ seconds on a test stand with no velocity command.

**Setup:**
- Place robot on a test stand with wheels off the ground
- Set `PID_VEL_KP = 0`, `PID_VEL_KI = 0` (disable outer loop)
- Set `cmd_vel.linear.x = 0`
- Monitor: `ros2 topic echo /balance_state` or rqt_plot `/balance_state/pitch_deg`

**Tuning steps:**

| Step | Action | Look for |
|------|--------|----------|
| 1 | Increase KP from 0 until robot oscillates | Fast oscillation |
| 2 | Set KP = ~50% of oscillation value | Underdamped but stable |
| 3 | Increase KD until oscillation damped | Smooth convergence |
| 4 | Verify: no steady-state offset at rest | If offset, add small KI |

**Starting values:** `KP=25.0, KI=0.0, KD=0.8`

**Runtime tuning (no reflash):**
```bash
ros2 service call /set_pid_gains sbr_interfaces/srv/SetPidGains \
  "{loop_id: 0, kp: 28.0, ki: 0.0, kd: 1.0}"
```

**Validation:** `ros2 topic echo /balance_state/pitch_deg` shows ±2° or less.

---

## Phase 2: Velocity (Outer) Loop

**Goal:** Robot moves at commanded speed and returns to rest when cmd_vel = 0.

**Setup:**
- Angle loop tuned and stable
- Place robot on flat floor with room to move
- Monitor: `/balance_state/velocity_mps` vs setpoint

**Tuning steps:**

| Step | Action | Look for |
|------|--------|----------|
| 1 | Increase KP from 0 | Robot starts responding to velocity command |
| 2 | If robot drifts and doesn't stop: add KI | Eliminates steady-state velocity error |
| 3 | If robot overshoots badly: add KD | Reduces overshoot |

**Starting values:** `KP=2.5, KI=0.3, KD=0.0`

**Caution:** High KI causes the robot to lean forward/back excessively.
If robot leans then falls: reduce KI first, then re-tune KP.

---

## Phase 3: Steering (Yaw) Loop

**Goal:** Robot tracks angular velocity command, turns accurately.

**Setup:**
- Both angle and velocity loops stable
- Command angular.z via teleop
- Monitor: `/balance_state/yaw_rate_dps`

**Tuning steps:** Similar to velocity loop. Start with KP only, add KD if oscillating.

**Starting values:** `KP=3.0, KI=0.0, KD=0.1`

---

## Equilibrium Angle Calibration

The robot may not balance at exactly 0° pitch due to:
- Centre of mass offset (battery placement, Pi 5 position)
- IMU mounting tilt

**Procedure:**
1. Let robot balance with `KP` working
2. Note steady-state pitch offset from rqt_plot
3. Update `IMU_ZERO_ANGLE_DEG` in `robot_params.h` with that offset
4. Reflash and verify robot stands vertical

---

## Common Failure Modes

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Robot falls immediately | KP too low or IMU axis wrong | Check IMU sign conventions |
| Fast, small oscillations | KD too low | Increase KD |
| Slow, growing oscillations | KP too high or KI too high | Reduce one at a time |
| Robot creeps forward/back at rest | Missing velocity KI or wrong zero angle | Calibrate zero angle first |
| Robot falls after 5–10 s | Integrator windup | Reduce KI or lower IMAX |
| Loud motor noise at rest | Derivative noise | Increase D-filter tau in `pid_init()` |
| IMU divergence fault | L3GD20 vs ICM mismatch | Verify axis orientations match |

---

## Live Monitoring with rqt_plot

```bash
# On Pi 5 or Host VM (with ROS2 domain visible):
ros2 run rqt_plot rqt_plot \
  /balance_state/pitch_deg \
  /balance_state/pitch_error_deg \
  /balance_state/angle_pid_output
```

---

## Recording a Tuning Session

```bash
ros2 bag record \
  /balance_state \
  /imu/data \
  /odom \
  /cmd_vel \
  -o tuning_session_$(date +%Y%m%d_%H%M%S)
```

Replay for offline analysis:
```bash
ros2 bag play tuning_session_YYYYMMDD_HHMMSS
```

---

## Committing Tuned Values

After validation, update `firmware/stm32/include/config/robot_params.h`:

```c
#define PID_ANGLE_KP    <your_tuned_value>
#define PID_ANGLE_KI    <your_tuned_value>
#define PID_ANGLE_KD    <your_tuned_value>
// ... etc
#define IMU_ZERO_ANGLE_DEG  <your_calibrated_value>
```

Then reflash STM32 and run the full validation:
```bash
python3 scripts/check_topics.py
```
