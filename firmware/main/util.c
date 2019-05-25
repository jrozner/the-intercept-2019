#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "util.h"

int hexchr2bin(char hex, uint8_t *out) {
    if (out == NULL) {
        return 0;
    }

    if (hex >= '0' && hex <= '9') {
        *out = hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        *out = hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        *out = hex - 'a' + 10;
    } else {
        return 0;
    }

    return 1;
}

uint8_t *hex_decode(char *in) {
    if (in == NULL || *in == '\0') {
        return NULL;
    }

    size_t len = strlen(in);
    if (len % 2 != 0) {
        return NULL;
    }

    len /= 2;

    uint8_t *out = malloc(len);
    int i;
    for (i = 0; i < len; i++) {
        uint8_t b1, b2;
        if (!hexchr2bin(in[i*2], &b1) || !hexchr2bin(in[i*2+1], &b2)) {
            return NULL;
        }

        out[i] = (b1 << 4) | b2;
    }

    return out;
}

const char hex_bytes[] = "0123456789abcdef";

void hex_encode(char *out, uint8_t *in, size_t len) {
    int i;
    for (i = 0; i < len; i++) {
        uint8_t byte = in[i];
        out[i * 2] = hex_bytes[byte >> 4];
        out[i * 2 + 1] = hex_bytes[byte & 0x0f];
    }

    out[i * 2] = '\0';
}

char *substr(char *s, size_t limit) {
    char *sub;
    if ((sub = malloc(limit+1)) == NULL) {
        return NULL;
    }

    strncpy(sub, s, limit);
    sub[limit] = '\0';

    return sub;
}
