/*
 * test_pid.c
 * Native unit tests for pid.c — runs on x86 host via:
 *   cd firmware/stm32 && pio test -e native
 *
 * Tests:
 *   1. Proportional-only: output = kp * error
 *   2. Integral accumulation and anti-windup clamping
 *   3. Derivative term sign
 *   4. Step response: P+I settles to zero steady-state error
 *   5. Output saturation clamping
 *   6. Reset clears state
 *   7. Runtime gain update resets integrator
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "pid.h"

#define DT       0.005f    /* 200 Hz sample period */
#define EPSILON  0.01f     /* floating-point comparison tolerance */

static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT_NEAR(a, b, tol, msg) do { \
    tests_run++; \
    if (fabsf((a) - (b)) <= (tol)) { \
        tests_passed++; \
        printf("  PASS  %s\n", msg); \
    } else { \
        printf("  FAIL  %s  (got %.4f, expected %.4f)\n", msg, (float)(a), (float)(b)); \
    } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  PASS  %s\n", msg); \
    } else { \
        printf("  FAIL  %s\n", msg); \
    } \
} while(0)

/* ── Test 1: P-only output ───────────────────────────────────── */
static void test_proportional_only(void)
{
    pid_t pid;
    pid_init(&pid, 10.0f, 0.0f, 0.0f, 100.0f, 1000.0f, 0.0f, DT);

    float out = pid_compute(&pid, 5.0f, 0.0f);   /* error = 5 */
    ASSERT_NEAR(out, 50.0f, EPSILON, "P-only: output = kp * error");

    out = pid_compute(&pid, -3.0f, 0.0f);         /* error = -3 */
    ASSERT_NEAR(out, -30.0f, EPSILON, "P-only: negative error");
}

/* ── Test 2: Integral accumulation ──────────────────────────── */
static void test_integral(void)
{
    pid_t pid;
    pid_init(&pid, 0.0f, 10.0f, 0.0f, 1000.0f, 10000.0f, 0.0f, DT);

    /* After 10 steps with error=1: integrator = 10 * DT = 0.05 */
    for (int i = 0; i < 10; i++) {
        pid_compute(&pid, 1.0f, 0.0f);
    }
    float out = pid_compute(&pid, 1.0f, 0.0f);
    /* integrator = 11*DT*1 = 0.055, I-term = ki * integrator = 0.55 */
    ASSERT_NEAR(out, 10.0f * 11 * DT, EPSILON, "I-term accumulates correctly");
}

/* ── Test 3: Anti-windup clamp ───────────────────────────────── */
static void test_antiwindup(void)
{
    pid_t pid;
    float imax = 5.0f;
    pid_init(&pid, 0.0f, 1.0f, 0.0f, imax, 10000.0f, 0.0f, DT);

    /* Drive integrator far past imax */
    for (int i = 0; i < 10000; i++) {
        pid_compute(&pid, 100.0f, 0.0f);
    }
    float out = pid_compute(&pid, 100.0f, 0.0f);
    /* Output should be clamped to ki * imax = 5 */
    ASSERT_TRUE(out <= imax + EPSILON, "Anti-windup: integrator clamped");
}

/* ── Test 4: Derivative sign ─────────────────────────────────── */
static void test_derivative_sign(void)
{
    pid_t pid;
    pid_init(&pid, 0.0f, 0.0f, 1.0f, 0.0f, 10000.0f, 0.0f, DT);

    pid_compute(&pid, 0.0f, 0.0f);     /* first call: prev_error = 0 */
    float out = pid_compute(&pid, 0.0f, 5.0f); /* error went from 0 to -5 */
    /* D-term = kd * (error - prev_error) / dt = 1 * (-5 - 0) / DT */
    float expected = 1.0f * (-5.0f - 0.0f) / DT;
    /* Clamped to output_max = 10000 but expected is -1000 */
    ASSERT_NEAR(out, expected, fabsf(expected) * 0.01f,
                "D-term: correct sign when error decreases");
}

/* ── Test 5: Output saturation ───────────────────────────────── */
static void test_output_clamp(void)
{
    pid_t pid;
    pid_init(&pid, 100.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, DT);

    float out = pid_compute(&pid, 1000.0f, 0.0f);  /* P-out = 100000 */
    ASSERT_NEAR(out, 1.0f, EPSILON, "Output clamped to output_max");

    out = pid_compute(&pid, -1000.0f, 0.0f);
    ASSERT_NEAR(out, -1.0f, EPSILON, "Output clamped to -output_max");
}

/* ── Test 6: Reset clears state ──────────────────────────────── */
static void test_reset(void)
{
    pid_t pid;
    pid_init(&pid, 1.0f, 1.0f, 0.0f, 100.0f, 1000.0f, 0.0f, DT);

    for (int i = 0; i < 100; i++) pid_compute(&pid, 10.0f, 0.0f);
    pid_reset(&pid);

    /* After reset, integrator = 0, prev_error = 0 */
    float out = pid_compute(&pid, 1.0f, 0.0f);   /* P-only: should be kp*1 */
    ASSERT_NEAR(out, 1.0f, EPSILON, "Reset: integrator cleared");
}

/* ── Test 7: Runtime gain update resets integrator ──────────── */
static void test_gain_update(void)
{
    pid_t pid;
    pid_init(&pid, 1.0f, 10.0f, 0.0f, 100.0f, 1000.0f, 0.0f, DT);

    for (int i = 0; i < 50; i++) pid_compute(&pid, 5.0f, 0.0f);

    pid_set_gains(&pid, 2.0f, 0.0f, 0.0f);    /* clear I-gain + reset    */
    float out = pid_compute(&pid, 1.0f, 0.0f); /* should be kp*error only */
    ASSERT_NEAR(out, 2.0f, EPSILON, "Gain update resets integrator");
}

/* ── Main ────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== pid unit tests ===\n\n");

    test_proportional_only();
    test_integral();
    test_antiwindup();
    test_derivative_sign();
    test_output_clamp();
    test_reset();
    test_gain_update();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
