# CLAUDE.md — ESP32 Firmware

This file gives Claude Code context for working in `firmware/esp32/`.

## What this project is

ESP32 WiFi transport bridge for the Yahboom MicroROS self-balancing robot.
This is a **pure relay** — it does NOT run microROS itself.

```
STM32 ──UART 921600──► ESP32 ──WiFi UDP──► Raspberry Pi 5 (microROS agent)
STM32 ◄──UART 921600── ESP32 ◄──WiFi UDP── Raspberry Pi 5
```

## Build system

PlatformIO with Arduino framework.

```bash
pio run -e esp32-bridge              # build
pio run -e esp32-bridge -t upload    # flash
pio device monitor                   # serial monitor @ 115200
```

## WiFi credential provisioning (REQUIRED before first use)

```bash
python3 scripts/provision_nvs.py \
  --port /dev/ttyUSB0 \
  --ssid "YourNetwork" \
  --password "YourPassword" \
  --agent-ip "192.168.1.xxx"    # Raspberry Pi 5 IP address
```

Credentials are stored in ESP32 NVS flash — never in source code.

## Key files

| File | Purpose |
|------|---------|
| `src/main.cpp` | WiFi init, UART↔UDP relay loop |
| `include/nvs_config.h` | NVS key names and struct |
| `scripts/provision_nvs.py` | Credential provisioning tool |

## Key constants (in main.cpp)

| Constant | Value | Notes |
|----------|-------|-------|
| `AGENT_UDP_PORT` | 8888 | Must match Pi 5 agent |
| `UART_BAUD` | 921600 | Must match STM32 |
| `UART_RX_PIN` | GPIO16 | From STM32 TX |
| `UART_TX_PIN` | GPIO17 | To STM32 RX |
| `WIFI_TIMEOUT_MS` | 15000 | 15 s connection timeout |

## LED status

| LED state | Meaning |
|-----------|---------|
| OFF | No WiFi |
| ON solid | WiFi connected, UDP active |

## Do not

- Hardcode SSID, password, or IP address in any source file
- Add business logic or data processing — this is a relay only
- Buffer more than one UART packet before forwarding
