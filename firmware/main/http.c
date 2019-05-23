#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sodium.h>
#include <esp_http_client.h>
#include <nvs.h>

#include "http.h"
#include "common.h"

char serial[12];
char *access_key = NULL;
uint8_t *secret_key = NULL;

int lookup_serial() {
    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    size_t serial_length;
    err = nvs_get_str(nvs, "serial", NULL, &serial_length);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(err);
    }

    err = nvs_get_str(nvs, "serial", serial, &serial_length);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            for (int i = 0; i < 11; i++) {
                char c = (esp_random() % 10) + '0';
                serial[i] = c;
            }
            serial[11] = 0;
            ESP_ERROR_CHECK(nvs_set_str(nvs, "serial", serial));
            ESP_ERROR_CHECK(nvs_commit(nvs));
        } else {
            ESP_ERROR_CHECK(err)
        }
    }

    nvs_close(nvs);
    return ESP_OK;
}

int lookup_access_key() {
    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    size_t key_length;
    err = nvs_get_str(nvs, "access_key", NULL, &key_length);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_OK;
        } else {
            ESP_ERROR_CHECK(err);
        }
    }

    if ((access_key = malloc(key_length)) == NULL) {
        nvs_close(nvs);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(nvs_get_str(nvs, "access_key", access_key, &key_length));

    nvs_close(nvs);
    return ESP_OK;
}

int set_access_key(char *val) {
    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "access_key", val));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    if (access_key != NULL) {
        free(access_key);
    }
    access_key = val;

    return ESP_OK;
}

int lookup_secret_key() {
    nvs_handle nvs;
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));

    size_t key_length;
    err = nvs_get_str(nvs, "secret_key", NULL, &key_length);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_OK;
        } else {
            ESP_ERROR_CHECK(err);
        }
    }

    if ((secret_key = malloc(key_length)) == NULL) {
        nvs_close(nvs);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(nvs_get_str(nvs, "secret_key", access_key, &key_length));

    nvs_close(nvs);
    return ESP_OK;
}

int set_secret_key(uint8_t *val, size_t len) {
    nvs_handle nvs;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, "secret_key", val, len));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    if (secret_key != NULL) {
        free(secret_key);
    }
    secret_key = val;

    return ESP_OK;
}

int generate_signature(unsigned char *out, unsigned char *key, char *method, char *body) {
    char *input;
    size_t input_len;
    //unsigned char hash[crypto_auth_hmacsha256_BYTES];

    if (asprintf(&input, "%s%s%s", method, serial, body) == -1) {
        return -1;
    }

    input_len = strlen(input);

    crypto_auth_hmacsha256(out, (unsigned char *)input, input_len, secret_key);

    free(input);
    return 0;
}
