#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <lwip/sockets.h>

#define RTP_HEADER_SIZE   12
#define RTP_LOCAL_PORT    5004
#define RTP_TIMESTAMP_DELTA 160

typedef struct __attribute__((packed)) {
    uint8_t  cc_version;
    uint8_t  marker_pt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

typedef struct {
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
    int      socket_fd;
    struct sockaddr_in remote_addr;
} rtp_ctx_t;

void rtp_init(rtp_ctx_t *ctx, const char *remote_ip, uint16_t remote_port);
int  rtp_send(rtp_ctx_t *ctx, const uint8_t *payload, uint16_t payload_len);
int  rtp_recv(rtp_ctx_t *ctx, uint8_t *payload, uint16_t *payload_len);
void rtp_close(rtp_ctx_t *ctx);

#endif
