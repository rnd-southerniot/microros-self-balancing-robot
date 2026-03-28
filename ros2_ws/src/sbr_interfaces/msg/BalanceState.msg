# BalanceState.msg
# Published by sbr_controller/pid_monitor_node at 100 Hz.
# Carries complete balance controller state for tuning and telemetry.

std_msgs/Header header

# ── Angle (inner) loop ────────────────────────────────────────
float32 pitch_deg          # current pitch angle (degrees, + = nose up)
float32 pitch_setpoint_deg # target pitch (usually 0 + velocity offset)
float32 pitch_error_deg    # setpoint - measured
float32 angle_pid_output   # output of angle PID (feeds velocity loop)

# ── Velocity (outer) loop ─────────────────────────────────────
float32 velocity_mps       # measured forward velocity (m/s)
float32 velocity_setpoint  # commanded velocity (from cmd_vel)
float32 velocity_error     # setpoint - measured
float32 vel_pid_output     # output of velocity PID

# ── Steering loop ─────────────────────────────────────────────
float32 yaw_rate_dps       # measured yaw rate (deg/s)
float32 steer_pid_output   # differential motor torque command

# ── Raw encoder odometry ──────────────────────────────────────
float32 left_rpm           # left motor RPM
float32 right_rpm          # right motor RPM

# ── IMU raw ───────────────────────────────────────────────────
float32 gyro_y_dps         # ICM-20948 pitch-axis gyro (deg/s)
float32 gyro_y_l3gd20_dps  # L3GD20 cross-check value

# ── Fault flags ───────────────────────────────────────────────
# Bit 0: IMU primary fault
# Bit 1: IMU secondary fault
# Bit 2: IMU divergence fault
# Bit 3: motor driver fault
# Bit 4: cmd_vel timeout (watchdog fired)
uint8 fault_flags
