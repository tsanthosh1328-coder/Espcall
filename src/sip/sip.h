#ifndef SIP_H
#define SIP_H

#include <stdint.h>
#include <lwip/sockets.h>

typedef enum {
    SIP_STATE_IDLE,
    SIP_STATE_REGISTERING,
    SIP_STATE_REGISTERED,
    SIP_STATE_CALLING,
    SIP_STATE_IN_CALL,
    SIP_STATE_HANGING_UP,
    SIP_STATE_ERROR
} sip_state_t;

typedef struct {
    const char *server;
    uint16_t    server_port;
    const char *username;
    const char *password;
    const char *local_ip;
    uint16_t    local_port;
    uint16_t    rtp_port;
} sip_config_t;

typedef struct {
    sip_config_t config;
    sip_state_t  state;
    int          socket_fd;
    uint32_t     call_id_num;
    uint32_t     tag_local;
    uint32_t     cseq;
    char         remote_rtp_ip[32];
    uint16_t     remote_rtp_port;
    struct sockaddr_in server_addr;
} sip_ctx_t;

void sip_init(sip_ctx_t *ctx, const sip_config_t *config);
int  sip_register(sip_ctx_t *ctx);
int  sip_call(sip_ctx_t *ctx, const char *number);
int  sip_bye(sip_ctx_t *ctx);
void sip_close(sip_ctx_t *ctx);

#endif
