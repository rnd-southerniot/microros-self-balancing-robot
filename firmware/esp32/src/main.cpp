/*
 * main.cpp — ESP32 microROS WiFi transport bridge
 *
 * Role: Relay microROS serial frames between STM32 (UART2) and
 *       the Raspberry Pi 5 microROS agent (UDP port 8888).
 *
 * Data flow:
 *   STM32 → UART2 RX → UDP TX → Pi 5 agent (microROS upstream)
 *   Pi 5 agent → UDP RX → UART2 TX → STM32 (microROS downstream)
 *
 * Credentials: loaded from NVS at boot — NEVER hardcoded here.
 * Provision with: python3 scripts/provision_nvs.py
 *
 * Yahboom lesson ref: microROS board env §2–5, ESP32 basic §15
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "nvs_config.h"

/* ── Configuration ───────────────────────────────────────────── */
#define AGENT_UDP_PORT      8888
#define UART_BAUD           921600
#define UART_RX_PIN         16      /* GPIO16 → UART2 RX from STM32 */
#define UART_TX_PIN         17      /* GPIO17 → UART2 TX to STM32   */
#define LED_STATUS_PIN      2       /* Onboard LED — WiFi status     */

#define WIFI_TIMEOUT_MS     15000   /* 15 s connection timeout       */
#define RECONNECT_INTERVAL  5000    /* Retry interval ms             */

/* ── Globals ─────────────────────────────────────────────────── */
static AsyncUDP udp;
static nvs_config_t cfg;
static bool wifi_connected = false;
static uint32_t last_reconnect_ms = 0;

/* ── Prototypes ──────────────────────────────────────────────── */
static void wifi_connect(void);
static void on_wifi_event(WiFiEvent_t event);
static void udp_rx_callback(AsyncUDPPacket &packet);

/* ── Setup ───────────────────────────────────────────────────── */
void setup(void)
{
    Serial.begin(115200);
    Serial.println("[sbr-bridge] booting...");

    /* Load WiFi credentials and agent IP from NVS */
    if (!nvs_config_load(&cfg)) {
        Serial.println("[sbr-bridge] FATAL: NVS credentials not found.");
        Serial.println("[sbr-bridge] Run: python3 scripts/provision_nvs.py");
        /* Halt — no safe default without credentials */
        while (1) { delay(1000); }
    }

    Serial.printf("[sbr-bridge] SSID: %s\n", cfg.ssid);
    Serial.printf("[sbr-bridge] Agent: %s:%d\n", cfg.agent_ip, AGENT_UDP_PORT);

    /* UART2 for STM32 comms */
    Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.printf("[sbr-bridge] UART2 @ %d baud (RX=%d TX=%d)\n",
                  UART_BAUD, UART_RX_PIN, UART_TX_PIN);

    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, LOW);

    /* WiFi event handler */
    WiFi.onEvent(on_wifi_event);

    wifi_connect();
}

/* ── Main loop ───────────────────────────────────────────────── */
void loop(void)
{
    /* ── STM32 → Pi 5: relay UART bytes to UDP ── */
    if (wifi_connected && Serial2.available()) {
        uint8_t buf[256];
        size_t n = Serial2.readBytes(buf, sizeof(buf));
        if (n > 0) {
            IPAddress agent_addr;
            agent_addr.fromString(cfg.agent_ip);
            udp.writeTo(buf, n, agent_addr, AGENT_UDP_PORT);
        }
    }

    /* ── Reconnect if WiFi dropped ── */
    if (!wifi_connected) {
        uint32_t now = millis();
        if (now - last_reconnect_ms > RECONNECT_INTERVAL) {
            last_reconnect_ms = now;
            Serial.println("[sbr-bridge] WiFi lost — reconnecting...");
            wifi_connect();
        }
    }
}

/* ── UDP RX callback: Pi 5 → STM32 ──────────────────────────── */
static void udp_rx_callback(AsyncUDPPacket &packet)
{
    /* Forward incoming UDP bytes to STM32 via UART2 */
    Serial2.write(packet.data(), packet.length());
}

/* ── WiFi connect ────────────────────────────────────────────── */
static void wifi_connect(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.password);

    Serial.print("[sbr-bridge] Connecting to WiFi");
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[sbr-bridge] WiFi OK. IP: %s\n",
                      WiFi.localIP().toString().c_str());
        wifi_connected = true;
        digitalWrite(LED_STATUS_PIN, HIGH);

        /* Start UDP listener */
        if (udp.listen(AGENT_UDP_PORT)) {
            udp.onPacket(udp_rx_callback);
            Serial.printf("[sbr-bridge] UDP listening on :%d\n",
                          AGENT_UDP_PORT);
        } else {
            Serial.println("[sbr-bridge] ERROR: UDP listen failed");
        }
    } else {
        Serial.println("[sbr-bridge] WiFi connect FAILED");
        wifi_connected = false;
        digitalWrite(LED_STATUS_PIN, LOW);
    }
}

/* ── WiFi event handler ──────────────────────────────────────── */
static void on_wifi_event(WiFiEvent_t event)
{
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        wifi_connected = false;
        digitalWrite(LED_STATUS_PIN, LOW);
        Serial.println("[sbr-bridge] WiFi disconnected");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        wifi_connected = true;
        digitalWrite(LED_STATUS_PIN, HIGH);
        Serial.printf("[sbr-bridge] IP: %s\n",
                      WiFi.localIP().toString().c_str());
        break;
    default:
        break;
    }
}
