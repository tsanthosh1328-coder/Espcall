#ifndef G711_H
#define G711_H

#include <stdint.h>

uint8_t g711_ulaw_encode(int16_t pcm_sample);
int16_t g711_ulaw_decode(uint8_t ulaw_byte);
void g711_encode_frame(const int16_t *pcm_buf, uint8_t *ulaw_buf, int num_samples);
void g711_decode_frame(const uint8_t *ulaw_buf, int16_t *pcm_buf, int num_samples);
void g711_test(void);

#endif
