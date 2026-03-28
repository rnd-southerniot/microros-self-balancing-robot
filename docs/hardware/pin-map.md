# Hardware Pin Map

**Board:** STM32F429I-DISC1 (STM32F429ZIT6 @ 180 MHz)  
**Confirmed from:** `dual-motor-controller-balancer` firmware source

---

## Motor 1 (Left — 520 encoder motor)

| Signal | GPIO | Timer / AF | Direction |
|--------|------|-----------|-----------|
| PWM speed | PB4 | TIM3_CH1 / AF2 | Output |
| Dir A (IN1) | PA5 | GPIO OUTPUT | Output |
| Dir B (IN2) | PA7 | GPIO OUTPUT | Output |
| Encoder A | PC4 | TIM encoder mode | Input |
| Encoder B | PC5 | TIM encoder mode | Input |

## Motor 2 (Right — 520 encoder motor)

| Signal | GPIO | Timer / AF | Direction |
|--------|------|-----------|-----------|
| PWM speed | PB7 | TIM4_CH2 / AF2 | Output |
| Dir A (IN1) | PD4 | GPIO OUTPUT | Output |
| Dir B (IN2) | PD5 | GPIO OUTPUT | Output |
| Encoder A | PB9 | TIM encoder mode | Input |
| Encoder B | PB8 | TIM encoder mode | Input |

**Encoder CPR:** 1320 counts/revolution (520 motor + gearbox)

---

## IMU: ICM-20948 Primary (I2C1)

| Signal | GPIO | Notes |
|--------|------|-------|
| SCL | PB6 | I2C1_SCL |
| SDA | PB9 | I2C1_SDA — **see conflict note below** |
| I2C address | 0x68 | AD0 = GND |
| Axes | 9 | Accel + Gyro + Mag |

## IMU: L3GD20 Secondary (SPI1, onboard Discovery)

| Signal | GPIO | Notes |
|--------|------|-------|
| CS | PC1 | Active low |
| INT1 | PA1 | Data ready interrupt |
| DRDY | PB2 | Data ready |
| SPI bus | SPI1 | Shared with onboard peripherals |

---

## HC-SR04 Ultrasonic

| Signal | GPIO | Notes |
|--------|------|-------|
| TRIG | PE9 | *Placeholder — confirm on chassis* |
| ECHO | PE11 | TIM1 input capture — *placeholder* |

---

## UART: STM32 ↔ ESP32

| Signal | GPIO | Notes |
|--------|------|-------|
| TX | PA9 | USART1_TX |
| RX | PA10 | USART1_RX |
| Baud rate | 921600 | 8N1 |

---

## LCD ILI9341 (onboard Discovery)

| Signal | GPIO | Notes |
|--------|------|-------|
| CS | PC2 | SPI5 |
| DC | PD13 | Data/command select |
| RST | PD12 | Reset |

---

## Onboard User Controls

| Signal | GPIO | Notes |
|--------|------|-------|
| LED Green | PG13 | Status indicator |
| LED Red | PG14 | Fault indicator |
| User Button | PA0 | Wake-up button |

---

## ⚠️ Known Pin Conflicts — Must Resolve Before Phase 1

### Conflict 1: PB7 — I2C1_SDA vs TIM4_CH2

`PB7` is used for **both** Motor 2 PWM (TIM4_CH2, AF2) and I2C1_SDA (AF4).  
These are alternate functions — only one can be active at a time.

**Resolution options (choose one):**

| Option | Change | Impact |
|--------|--------|--------|
| A | Move ICM-20948 to I2C3 (PH7/PH8 or PC9) | Requires rewiring IMU |
| B | Move Motor 2 PWM to TIM4_CH1 on PB6 | Requires motor board change |
| C | Move ICM-20948 SDA to PB9 (I2C1 alt) | Check if PB9 is free |

**Action:** Resolve in CubeMX before hardware bring-up. Check schematic for available I2C pads.

### Conflict 2: Encoder timer assignments

`TIM8` and `TIM5` are listed as encoder timers — verify in CubeMX that these:
- Support encoder mode on F429
- Are not already claimed by other peripherals (FMC, LTDC, etc.)

**Action:** Open CubeMX with `STM32F429ZIT6`, assign encoder mode to available TIMx, update `pin_config.h`.

---

## Timer Allocation Summary

| Timer | Function | GPIO Pins |
|-------|----------|-----------|
| TIM3 | Motor 1 PWM CH1 | PB4 |
| TIM4 | Motor 2 PWM CH2 | PB7 ⚠️ |
| TIM8 | Encoder M1 (proposed) | PC4, PC5 |
| TIM5 | Encoder M2 (proposed) | PB8, PB9 |
| TIM1 | HC-SR04 input capture | PE9, PE11 |
| SPI1 | L3GD20 | Onboard |
| SPI5 | ILI9341 LCD | Onboard |
| I2C1 | ICM-20948 | PB6, PB7 ⚠️ |
| USART1 | ESP32 bridge | PA9, PA10 |

---

## ESP32 Pin Map (WiFi Bridge)

| Signal | GPIO | Notes |
|--------|------|-------|
| UART2 RX | GPIO16 | From STM32 TX |
| UART2 TX | GPIO17 | To STM32 RX |
| Status LED | GPIO2 | Onboard LED |
| Baud rate | 921600 | Must match STM32 |

---

## Raspberry Pi 5 Connections

| Interface | Notes |
|-----------|-------|
| WiFi | 5 GHz recommended — microROS UDP latency sensitive |
| Ethernet | Recommended for development / RVIZ2 streaming |
| USB-A | ESP32 serial (provisioning only) |
| CSI | Camera (Phase 6 — vision module) |

---

## Reference

- Yahboom lessons: https://www.yahboom.net/study/SBR-microROS
- STM32F429 datasheet: https://www.st.com/en/microcontrollers-microprocessors/stm32f429zi.html
- ICM-20948 datasheet: https://invensense.tdk.com/products/motion-tracking/9-axis/icm-20948/
