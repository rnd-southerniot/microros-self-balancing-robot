/*
 * nvs_config.cpp — NVS credential storage for ESP32 WiFi bridge
 *
 * Reads/writes WiFi SSID, password, and agent IP from ESP32 NVS flash.
 */

#include "nvs_config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>

static bool nvs_initialized = false;

static bool ensure_nvs_init(void)
{
    if (nvs_initialized) return true;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    nvs_initialized = (err == ESP_OK);
    return nvs_initialized;
}

bool nvs_config_load(nvs_config_t *cfg)
{
    if (!cfg || !ensure_nvs_init()) return false;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return false;

    size_t len;
    bool ok = true;

    len = sizeof(cfg->ssid);
    if (nvs_get_str(handle, NVS_KEY_SSID, cfg->ssid, &len) != ESP_OK)
        ok = false;

    len = sizeof(cfg->password);
    if (nvs_get_str(handle, NVS_KEY_PASSWORD, cfg->password, &len) != ESP_OK)
        ok = false;

    len = sizeof(cfg->agent_ip);
    if (nvs_get_str(handle, NVS_KEY_AGENT_IP, cfg->agent_ip, &len) != ESP_OK)
        ok = false;

    nvs_close(handle);
    return ok;
}

bool nvs_config_save(const nvs_config_t *cfg)
{
    if (!cfg || !ensure_nvs_init()) return false;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;

    bool ok = true;
    if (nvs_set_str(handle, NVS_KEY_SSID, cfg->ssid) != ESP_OK) ok = false;
    if (nvs_set_str(handle, NVS_KEY_PASSWORD, cfg->password) != ESP_OK) ok = false;
    if (nvs_set_str(handle, NVS_KEY_AGENT_IP, cfg->agent_ip) != ESP_OK) ok = false;

    if (ok) nvs_commit(handle);
    nvs_close(handle);
    return ok;
}

void nvs_config_erase(void)
{
    if (!ensure_nvs_init()) return;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
}
