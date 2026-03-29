/*
 * ESP32 microROS WiFi transport bridge
 * UART (STM32) <-> WiFi UDP (ROS2 agent)
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define AGENT_PORT  8888
#define UART_BAUD   921600
#define UART_RX     16
#define UART_TX     17
#define LED         2
#define WIFI_TMO    15000
#define RECONN_MS   5000

static WiFiUDP udp;
static IPAddress agentIP;
static bool wifiUp = false;
static uint32_t lastReconn = 0;

static void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin("Auritro", "A24042017S");
    Serial.print("[sbr] Connecting");
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TMO) {
        delay(500); Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        wifiUp = true;
        digitalWrite(LED, HIGH);
        udp.begin(AGENT_PORT);
        Serial.printf("[sbr] WiFi OK IP:%s UDP:%d\n",
            WiFi.localIP().toString().c_str(), AGENT_PORT);
    } else {
        wifiUp = false;
        Serial.println("[sbr] WiFi FAIL");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[sbr] boot");
    agentIP.fromString("10.10.8.110");
    Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    connectWiFi();
}

void loop() {
    // UART -> UDP
    if (wifiUp && Serial2.available()) {
        uint8_t buf[512];
        size_t n = Serial2.readBytes(buf, sizeof(buf));
        if (n > 0) {
            udp.beginPacket(agentIP, AGENT_PORT);
            udp.write(buf, n);
            udp.endPacket();
        }
    }
    // UDP -> UART
    int ps = udp.parsePacket();
    if (ps > 0) {
        uint8_t buf[512];
        int n = udp.read(buf, sizeof(buf));
        if (n > 0) Serial2.write(buf, n);
    }
    // Reconnect
    if (WiFi.status() != WL_CONNECTED && wifiUp) {
        wifiUp = false;
        digitalWrite(LED, LOW);
        Serial.println("[sbr] WiFi lost");
    }
    if (!wifiUp && millis() - lastReconn > RECONN_MS) {
        lastReconn = millis();
        connectWiFi();
    }
    delay(1);  // yield to WDT + WiFi task
}
