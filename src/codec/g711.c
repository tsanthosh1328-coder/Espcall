#include "codec/g711.h"
#include <stdio.h>

#define BIAS    132
#define CLIP    32635

uint8_t g711_ulaw_encode(int16_t pcm_sample)
{
    int sign;
    int seg;
    int mantissa;
    int biased;
    uint8_t ulaw_byte;

    sign = 0;
    biased = pcm_sample;
    if (biased < 0) {
        biased = -biased;
        sign = 0x80;
    }

    if (biased > CLIP) {
        biased = CLIP;
    }

    biased += BIAS;

    if (biased <= 0xFF) {
        seg = 0;
    } else if (biased <= 0x1FF) {
        seg = 1;
    } else if (biased <= 0x3FF) {
        seg = 2;
    } else if (biased <= 0x7FF) {
        seg = 3;
    } else if (biased <= 0xFFF) {
        seg = 4;
    } else if (biased <= 0x1FFF) {
        seg = 5;
    } else if (biased <= 0x3FFF) {
        seg = 6;
    } else {
        seg = 7;
    }

    mantissa = (biased >> (seg + 3)) & 0x0F;
    ulaw_byte = ~(uint8_t)(sign | (seg << 4) | mantissa);

    return ulaw_byte;
}

int16_t g711_ulaw_decode(uint8_t ulaw_byte)
{
    int sign;
    int exponent;
    int mantissa;
    int t;

    ulaw_byte = ~ulaw_byte;

    sign = (ulaw_byte & 0x80);
    exponent = (ulaw_byte >> 4) & 0x07;
    mantissa = (ulaw_byte & 0x0F);

    t = (mantissa << 3) + BIAS;
    t <<= exponent;
    t -= BIAS;

    if (sign) {
        t = -t;
    }

    return (int16_t)t;
}

void g711_encode_frame(const int16_t *pcm_buf, uint8_t *ulaw_buf, int num_samples)
{
    for (int i = 0; i < num_samples; i++) {
        ulaw_buf[i] = g711_ulaw_encode(pcm_buf[i]);
    }
}

void g711_decode_frame(const uint8_t *ulaw_buf, int16_t *pcm_buf, int num_samples)
{
    for (int i = 0; i < num_samples; i++) {
        pcm_buf[i] = g711_ulaw_decode(ulaw_buf[i]);
    }
}

void g711_test(void)
{
    int16_t test_values[] = {0, 100, -100, 1000, -1000, 16383, -16383};
    int num = sizeof(test_values) / sizeof(test_values[0]);

    printf("=== G.711 mu-law codec test ===\n");

    for (int i = 0; i < num; i++) {
        int16_t original = test_values[i];
        uint8_t encoded = g711_ulaw_encode(original);
        int16_t decoded = g711_ulaw_decode(encoded);

        printf("  %6d -> 0x%02X -> %6d\n", (int)original, encoded, (int)decoded);
    }

    printf("=== End of test ===\n");
}
