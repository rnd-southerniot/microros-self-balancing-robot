# CLAUDE.md — STM32 Firmware

This file gives Claude Code context for working in `firmware/stm32/`.

## What this project is

STM32F429I-DISC1 real-time balance control firmware for the Yahboom
MicroROS self-balancing robot. Runs at 200 Hz, publishes sensor data
to ROS2 via microROS (relayed through ESP32 WiFi bridge).

## Build system

PlatformIO. Two environments:
- `stm32f429` — builds for hardware (arm-none-eabi)
- `native`    — builds and runs unit tests on x86

```bash
pio run -e stm32f429           # build firmware
pio run -e stm32f429 -t upload # build + flash via ST-Link
pio test -e native             # run unit tests on host
```

## Key files

| File | Purpose |
|------|---------|
| `include/config/robot_params.h` | ALL physical constants, gains, rates — edit here |
| `include/config/pin_config.h` | ALL GPIO/timer assignments |
| `include/imu/icm20948.h` | ICM-20948 driver API |
| `include/imu/imu_filter.h` | Complementary filter API |
| `include/pid/pid.h` | Generic PID API |
| `src/pid/pid.c` | PID implementation (complete) |
| `test/native/test_pid.c` | PID unit tests |

## Hardware

- MCU: STM32F429ZIT6 @ 180 MHz
- IMU primary: ICM-20948 (I2C1 @ 0x68) — 9-axis
- IMU secondary: L3GD20 (SPI1) — onboard Discovery, cross-check only
- Motors: 520 encoder motors via TIM3/TIM4 PWM + GPIO H-bridge
- UART to ESP32: USART1 @ 921600

## Coding conventions

- C11 standard
- HAL-based (no LL drivers unless performance requires)
- All magic numbers in `robot_params.h` or `pin_config.h`
- Every public function documented in its `.h` file
- Unit-testable logic in separate `.c` files (no HAL calls in PID/filter math)

## Known issues / TODOs

- PB7 pin conflict: I2C1_SDA vs TIM4_CH2 — must resolve in CubeMX
- Encoder timer assignments (TIM8/TIM5) are placeholders
- HC-SR04 pins are placeholders — confirm on chassis
- microROS library integration not yet started (Phase 3)

## Do not

- Hardcode WiFi credentials or IP addresses
- Add HAL calls inside `pid.c` or `imu_filter.c` (breaks native tests)
- Change `robot_params.h` gains without committing a tuning note in git message
