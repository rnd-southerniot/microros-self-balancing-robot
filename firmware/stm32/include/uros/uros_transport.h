#pragma once

/*
 * uros_transport.h
 * Custom UART transport for microROS over USART1 → ESP32 WiFi bridge.
 * The ESP32 relays UART bytes to/from the microROS agent via UDP.
 */

#ifdef UROS_ENABLED

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Register custom UART transport with microROS.
 *         Must be called before rcl_init().
 * @return true on success.
 */
bool uros_transport_init(void);

#endif /* UROS_ENABLED */
