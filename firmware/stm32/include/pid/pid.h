#pragma once

/*
 * pid.h
 * Generic PID controller with:
 *   - Anti-windup (integral clamping)
 *   - Derivative filter (low-pass on D-term to reduce noise sensitivity)
 *   - Output clamping
 *   - Runtime gain update (for SetPidGains.srv tuning via ROS2)
 *
 * Yahboom lesson ref: ESP32 basic §9 (PID controls car speed)
 */

#include <stdint.h>

/* ── PID instance ────────────────────────────────────────────── */
typedef struct {
    /* Gains */
    float kp;
    float ki;
    float kd;

    /* State */
    float integrator;       /* accumulated integral term              */
    float prev_error;       /* previous error (for derivative)        */
    float prev_derivative;  /* filtered derivative (D filter state)   */

    /* Limits */
    float integrator_max;   /* anti-windup clamp (symmetric ±)        */
    float output_max;       /* output clamp (symmetric ±)             */

    /* Derivative filter: first-order low-pass */
    float d_filter_tau;     /* time constant [s]. 0 = no filter.      */
    float dt;               /* sample period [s]                      */
} pid_t;

/* ── API ─────────────────────────────────────────────────────── */

/**
 * @brief  Initialise PID instance. Must be called before first use.
 * @param  pid            PID state struct.
 * @param  kp, ki, kd     Gains.
 * @param  integrator_max Anti-windup clamp (applied ±).
 * @param  output_max     Output saturation clamp (applied ±).
 * @param  d_filter_tau   D-term low-pass time constant [s]. 0 = off.
 * @param  dt             Sample period [s] = 1.0f / loop_rate_hz.
 */
void pid_init(pid_t *pid,
              float kp, float ki, float kd,
              float integrator_max,
              float output_max,
              float d_filter_tau,
              float dt);

/**
 * @brief  Compute PID output for one time step.
 * @param  pid       PID state (updated in-place).
 * @param  setpoint  Desired value.
 * @param  measured  Current measured value.
 * @return PID output, clamped to ±output_max.
 */
float pid_compute(pid_t *pid, float setpoint, float measured);

/**
 * @brief  Reset integrator and derivative state.
 *         Call when switching from disabled to enabled, or on e-stop.
 */
void pid_reset(pid_t *pid);

/**
 * @brief  Update gains at runtime (from ROS2 SetPidGains.srv).
 *         Resets integrator to avoid windup on gain change.
 */
void pid_set_gains(pid_t *pid, float kp, float ki, float kd);

/**
 * @brief  Get current gains (for telemetry publishing).
 */
void pid_get_gains(const pid_t *pid, float *kp, float *ki, float *kd);
