/*
 * nvs_config.h + nvs_config.cpp
 * Reads WiFi SSID, password, and agent IP from ESP32 NVS flash.
 * No credentials are ever hardcoded in source.
 *
 * Provisioning: python3 scripts/provision_nvs.py \
 *     --port /dev/ttyUSB0 \
 *     --ssid "MyNet" --password "MyPass" --agent-ip "192.168.1.100"
 */

#pragma once
#include <stdbool.h>

#define NVS_NAMESPACE       "sbr_cfg"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASSWORD    "password"
#define NVS_KEY_AGENT_IP    "agent_ip"

#ifndef MAX_SSID_LEN
#define MAX_SSID_LEN        64
#endif
#define MAX_PASSWORD_LEN    64
#define MAX_IP_LEN          16

typedef struct {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWORD_LEN];
    char agent_ip[MAX_IP_LEN];
} nvs_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Load credentials from NVS.
 * @return true if all keys found and loaded, false if any missing.
 */
bool nvs_config_load(nvs_config_t *cfg);

/**
 * @brief  Save credentials to NVS (called by provision script flow).
 */
bool nvs_config_save(const nvs_config_t *cfg);

/**
 * @brief  Erase all sbr_cfg NVS keys.
 */
void nvs_config_erase(void);

#ifdef __cplusplus
}
#endif
