/*
 * test_imu_filter.c
 * Native unit tests for complementary filter (imu_filter.h)
 * Run: cd firmware/stm32 && pio test -e native
 *
 * Tests:
 *   1. Filter initialises with correct alpha and dt
 *   2. Pure gyro input: angle integrates correctly
 *   3. Filter converges to accel angle from arbitrary initial pitch
 *   4. Cross-check passes when gyros agree
 *   5. Cross-check flags fault when gyros diverge > threshold
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

/* Stub robot_params for native build */
#define L3GD20_DIVERGE_THRESHOLD 5.0f

#include "imu_filter.h"

static int tests_run = 0, tests_passed = 0;

#define ASSERT_NEAR(a, b, tol, msg) do { \
    tests_run++; \
    if (fabsf((float)(a) - (float)(b)) <= (float)(tol)) { \
        tests_passed++; printf("  PASS  %s\n", msg); \
    } else { \
        printf("  FAIL  %s  (got %.4f, expected %.4f)\n", msg, (float)(a), (float)(b)); \
    } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS  %s\n", msg); } \
    else       { printf("  FAIL  %s\n", msg); } \
} while(0)

/* ── Test 1: Initialisation ──────────────────────────────────── */
static void test_init(void)
{
    imu_filter_t f;
    imu_filter_init(&f, 0.98f, 0.002f);
    ASSERT_NEAR(f.alpha, 0.98f, 1e-5f, "Filter: alpha stored correctly");
    ASSERT_NEAR(f.dt,    0.002f, 1e-6f, "Filter: dt stored correctly");
    ASSERT_NEAR(f.pitch, 0.0f, 1e-5f,  "Filter: pitch initialised to 0");
    ASSERT_TRUE(f.initialised, "Filter: initialised flag set");
}

/* ── Test 2: Zero accel noise — gyro integration ─────────────── */
static void test_gyro_integration(void)
{
    imu_filter_t f;
    float dt = 0.002f;  /* 500 Hz */
    imu_filter_init(&f, 1.0f, dt);   /* alpha=1.0 = pure gyro */

    icm20948_data_t data;
    memset(&data, 0, sizeof(data));

    /* Constant 10 deg/s pitch rate for 100 steps = 2 degrees */
    data.gyro_y  = 10.0f;   /* pitch rate */
    data.accel_z = 1.0f;    /* gravity along Z — level */

    for (int i = 0; i < 100; i++) {
        imu_filter_update(&f, &data);
    }
    /* Expected: 10 * 100 * 0.002 = 2.0 degrees */
    ASSERT_NEAR(f.pitch, 2.0f, 0.05f, "Gyro integration: 2 degrees in 100 steps");
}

/* ── Test 3: Filter converges to accel angle ─────────────────── */
static void test_convergence(void)
{
    imu_filter_t f;
    float dt = 0.002f;
    imu_filter_init(&f, 0.98f, dt);

    /* Simulate robot tilted at 10 degrees — accel reflects this */
    float tilt_rad = 10.0f * (float)M_PI / 180.0f;
    icm20948_data_t data;
    memset(&data, 0, sizeof(data));
    data.accel_x = sinf(tilt_rad);   /* component along X when pitched  */
    data.accel_z = cosf(tilt_rad);
    data.gyro_y  = 0.0f;             /* stationary — no gyro rate       */

    for (int i = 0; i < 5000; i++) { /* 10 seconds at 500 Hz */
        imu_filter_update(&f, &data);
    }
    /* Should converge to ~10 degrees */
    ASSERT_NEAR(f.pitch, 10.0f, 0.5f, "Filter converges to accel angle (10 deg)");
}

/* ── Test 4: Cross-check passes when gyros agree ─────────────── */
static void test_crosscheck_pass(void)
{
    bool ok = imu_filter_cross_check(10.0f, 10.5f);  /* diff = 0.5 < 5 */
    ASSERT_TRUE(ok, "Cross-check passes: gyros agree within threshold");
}

/* ── Test 5: Cross-check flags fault on divergence ───────────── */
static void test_crosscheck_fail(void)
{
    bool ok = imu_filter_cross_check(10.0f, 18.0f);  /* diff = 8 > 5 */
    ASSERT_TRUE(!ok, "Cross-check fails: gyros diverge > 5 deg/s");
}

/* ── Main ────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== imu_filter unit tests ===\n\n");
    test_init();
    test_gyro_integration();
    test_convergence();
    test_crosscheck_pass();
    test_crosscheck_fail();
    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
