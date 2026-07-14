#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio/audio.h"
#include "codec/g711.h"
#include "rtp/rtp.h"
#include "sip/sip.h"

void app_main(void)
{
    printf("EspCall booting...\n");

    // Phase 1: Audio I/O init (I2S mic + speaker)
    // audio_init();

    // Phase 2: Codec (G.711 encode/decode)
    g711_test();

    // Phase 3: RTP stack
    // rtp_init();

    // Phase 4+5: SIP client
    // sip_init();

    printf("EspCall ready.\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
