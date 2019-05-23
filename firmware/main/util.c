#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "util.h"

uint8_t *hex_decode(char *in) {
    size_t input_length = strlen(in);

    if (input_length < 0) {
        return NULL;
    }

    if ((input_length % 2) != 0) {
        return NULL;
    }

    uint8_t *out;
    if ((out = calloc(input_length / 2, sizeof(uint8_t))) == NULL) {
        return NULL;
    }

    int i = 0;
    for (char *s = in; *s != '\0'; s += 2) {
        sscanf(s, "%2x", (int *)out+i);
    }

    return out;
}
