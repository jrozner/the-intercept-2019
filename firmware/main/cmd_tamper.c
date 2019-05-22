#include <nvs.h>
#include "cmd_tamper.h"
#include "common.h"

uint8_t get_tamper_nvs(){
    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    uint8_t val;
    err = nvs_get_u8(nvs, "tamper", &val);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_ERROR_CHECK(nvs_set_u8(nvs, "tamper", 0));
            ESP_ERROR_CHECK(nvs_commit(nvs));
            val = 0;
        } else {
            ESP_ERROR_CHECK(err);
        }
    }
    nvs_close(nvs);
    return val;
}

void set_tamper_nvs(uint8_t val) {
    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    uint8_t read_val;
    err = nvs_get_u8(nvs, "tamper", &read_val);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(nvs_set_u8(nvs, "tamper", val));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

