/*
 * uros_transport.c
 * Custom UART transport for micro-ROS over USART1.
 *
 * Data path: STM32 USART1 → ESP32 UART2 → WiFi UDP → ROS2 agent
 *
 * Implements the four transport callbacks required by micro-XRCE-DDS:
 *   open, close, write, read
 */

#ifdef UROS_ENABLED

#include "uros_transport.h"
#include "stm32f4xx_hal.h"
#include <uxr/client/transport.h>
#include <rmw_microxrcedds_c/config.h>

/* UART handle from main.c */
extern UART_HandleTypeDef *uros_uart_handle(void);

/* ── Transport callbacks ────────────────────────────────────── */

static bool uart_transport_open(uxrCustomTransport *transport)
{
    (void)transport;
    /* UART already initialised in main.c */
    return true;
}

static bool uart_transport_close(uxrCustomTransport *transport)
{
    (void)transport;
    return true;
}

static size_t uart_transport_write(uxrCustomTransport *transport,
                                   const uint8_t *buf, size_t len, uint8_t *err)
{
    (void)transport;
    UART_HandleTypeDef *huart = uros_uart_handle();

    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)buf,
                                                  (uint16_t)len, 100);
    if (status == HAL_OK) {
        *err = 0;
        return len;
    }
    *err = 1;
    return 0;
}

static size_t uart_transport_read(uxrCustomTransport *transport,
                                  uint8_t *buf, size_t len, int timeout_ms,
                                  uint8_t *err)
{
    (void)transport;
    UART_HandleTypeDef *huart = uros_uart_handle();

    /* Try to receive with timeout */
    HAL_StatusTypeDef status = HAL_UART_Receive(huart, buf, (uint16_t)len,
                                                 (uint32_t)timeout_ms);
    *err = 0;

    if (status == HAL_OK) {
        return len;
    } else if (status == HAL_TIMEOUT) {
        /* Return whatever bytes were received */
        uint16_t remaining = __HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) ? 0 :
                             huart->RxXferSize - huart->RxXferCount;
        return remaining;
    }
    *err = 1;
    return 0;
}

/* ── Public API ─────────────────────────────────────────────── */

bool uros_transport_init(void)
{
    rmw_uros_set_custom_transport(
        true,   /* framing = true (stream-oriented transport) */
        NULL,   /* no user args */
        uart_transport_open,
        uart_transport_close,
        uart_transport_write,
        uart_transport_read
    );
    return true;
}

#endif /* UROS_ENABLED */
