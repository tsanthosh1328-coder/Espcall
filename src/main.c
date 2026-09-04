#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "audio/audio.h"
#include "codec/g711.h"
#include "rtp/rtp.h"
#include "sip/sip.h"

#define WIFI_SSID     ""
#define WIFI_PASS     ""
#define SIP_USERNAME  ""
#define SIP_PASS      ""
#define SIP_SERVER    ""
#define SIP_PORT      5060
#define RTP_PORT      5004
#define SIP_LPORT     5080

static EventGroupHandle_t s_wifi_event_group;
static char local_ip[16];

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, local_ip, sizeof(local_ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WEP,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        printf("EspCall: WiFi connected, IP=%s\n", local_ip);
    } else if (bits & WIFI_FAIL_BIT) {
        printf("EspCall: WiFi connection failed\n");
    }
}

void app_main(void)
{
    printf("EspCall booting...\n");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Phase 1: Audio I/O init (I2S mic + speaker)
    // audio_init();

    // Phase 2: Codec (G.711 encode/decode)
    // g711_test();

    wifi_init_sta();

    sip_config_t sip_cfg = {
        .server = SIP_SERVER,
        .server_port = SIP_PORT,
        .username = SIP_USERNAME,
        .password = SIP_PASS,
        .local_ip = local_ip,
        .local_port = SIP_LPORT,
        .rtp_port = RTP_PORT,
    };

    sip_ctx_t sip_ctx;
    sip_init(&sip_ctx, &sip_cfg);

    // Phase 3: RTP stack
    // rtp_init();

    if (sip_register(&sip_ctx) == 0) {
        printf("EspCall: SIP registered successfully!\n");
    } else {
        printf("EspCall: SIP registration failed.\n");
    }

    printf("EspCall ready.\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
