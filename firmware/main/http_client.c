#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sodium.h>
#include <esp_http_client.h>
#include "http_client.h"

unsigned char secret_key[64];
char access_key[129];

int generate_signature(unsigned char *out, unsigned char *key, char *method, char *serial, char *body) {
    char *input;
    size_t input_len;
    //unsigned char hash[crypto_auth_hmacsha256_BYTES];

    if (asprintf(&input, "%s%s%s", method, serial, body) == -1) {
        return -1;
    }

    input_len = strlen(input);

    crypto_auth_hmacsha256(out, (unsigned char *)input, input_len, key);

    free(input);
    return 0;
}
