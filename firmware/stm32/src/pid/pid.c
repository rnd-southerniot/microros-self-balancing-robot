/*
 * pid.c
 * Generic PID controller implementation.
 * See pid.h for full API documentation.
 */

#include "pid.h"
#include <math.h>

/* ── Internal helpers ────────────────────────────────────────── */

static float clamp(float val, float limit)
{
    if (val >  limit) return  limit;
    if (val < -limit) return -limit;
    return val;
}

/* ── Public API ──────────────────────────────────────────────── */

void pid_init(pid_t *pid,
              float kp, float ki, float kd,
              float integrator_max,
              float output_max,
              float d_filter_tau,
              float dt)
{
    pid->kp             = kp;
    pid->ki             = ki;
    pid->kd             = kd;
    pid->integrator_max = integrator_max;
    pid->output_max     = output_max;
    pid->d_filter_tau   = d_filter_tau;
    pid->dt             = dt;

    pid->integrator      = 0.0f;
    pid->prev_error      = 0.0f;
    pid->prev_derivative = 0.0f;
}

float pid_compute(pid_t *pid, float setpoint, float measured)
{
    float error = setpoint - measured;

    /* Proportional term */
    float p_term = pid->kp * error;

    /* Integral term with anti-windup clamp */
    pid->integrator += error * pid->dt;
    pid->integrator  = clamp(pid->integrator, pid->integrator_max);
    float i_term     = pid->ki * pid->integrator;

    /* Derivative term with optional first-order low-pass filter
     * D_raw = (error - prev_error) / dt
     * D_filt = (tau * D_prev + dt * D_raw) / (tau + dt)
     * When tau == 0: no filtering, use raw derivative directly.
     */
    float d_raw = (error - pid->prev_error) / pid->dt;
    float d_term;

    if (pid->d_filter_tau > 0.0f) {
        float alpha = pid->dt / (pid->d_filter_tau + pid->dt);
        pid->prev_derivative = pid->prev_derivative
                               + alpha * (d_raw - pid->prev_derivative);
        d_term = pid->kd * pid->prev_derivative;
    } else {
        d_term = pid->kd * d_raw;
    }

    pid->prev_error = error;

    /* Sum and clamp output */
    float output = p_term + i_term + d_term;
    return clamp(output, pid->output_max);
}

void pid_reset(pid_t *pid)
{
    pid->integrator      = 0.0f;
    pid->prev_error      = 0.0f;
    pid->prev_derivative = 0.0f;
}

void pid_set_gains(pid_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid_reset(pid);   /* reset state to prevent windup on gain change */
}

void pid_get_gains(const pid_t *pid, float *kp, float *ki, float *kd)
{
    *kp = pid->kp;
    *ki = pid->ki;
    *kd = pid->kd;
}
