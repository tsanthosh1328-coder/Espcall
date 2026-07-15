#include "rtp/rtp.h"
#include <string.h>
#include <stdio.h>
#include <lwip/inet.h>
#include <esp_system.h>

void rtp_init(rtp_ctx_t *ctx, const char *remote_ip, uint16_t remote_port)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->socket_fd < 0) {
        printf("RTP: socket create failed\n");
        ctx->socket_fd = -1;
        return;
    }

    int opt = 1;
    setsockopt(ctx->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RTP_LOCAL_PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctx->socket_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        printf("RTP: bind to port %d failed\n", RTP_LOCAL_PORT);
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return;
    }

    memset(&ctx->remote_addr, 0, sizeof(ctx->remote_addr));
    ctx->remote_addr.sin_family = AF_INET;
    ctx->remote_addr.sin_port = htons(remote_port);
    inet_aton(remote_ip, &ctx->remote_addr.sin_addr);

    ctx->ssrc = esp_random();
    ctx->seq = 0;
    ctx->timestamp = 0;

    printf("RTP: initialized, SSRC=0x%08lX, remote=%s:%u\n",
           (unsigned long)ctx->ssrc, remote_ip, (unsigned)remote_port);
}

int rtp_send(rtp_ctx_t *ctx, const uint8_t *payload, uint16_t payload_len)
{
    if (ctx->socket_fd < 0) {
        return -1;
    }

    rtp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.cc_version = (2 << 6);
    hdr.marker_pt = 0;
    hdr.seq = htons(ctx->seq++);
    hdr.timestamp = htonl(ctx->timestamp);
    ctx->timestamp += RTP_TIMESTAMP_DELTA;
    hdr.ssrc = htonl(ctx->ssrc);

    static uint8_t buf[RTP_HEADER_SIZE + 160];
    memcpy(buf, &hdr, RTP_HEADER_SIZE);
    memcpy(buf + RTP_HEADER_SIZE, payload, payload_len);

    ssize_t sent = sendto(ctx->socket_fd, buf, sizeof(buf), 0,
                          (struct sockaddr *)&ctx->remote_addr,
                          sizeof(ctx->remote_addr));

    if (sent < 0) {
        return -1;
    }

    return (int)sent;
}

int rtp_recv(rtp_ctx_t *ctx, uint8_t *payload, uint16_t *payload_len)
{
    if (ctx->socket_fd < 0) {
        return -1;
    }

    uint8_t buf[2048];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    ssize_t n = recvfrom(ctx->socket_fd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fromlen);
    if (n < 0) {
        return -1;
    }

    if (n < RTP_HEADER_SIZE) {
        return -1;
    }

    uint16_t data_len = (uint16_t)(n - RTP_HEADER_SIZE);
    memcpy(payload, buf + RTP_HEADER_SIZE, data_len);
    *payload_len = data_len;

    return (int)n;
}

void rtp_close(rtp_ctx_t *ctx)
{
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    printf("RTP: closed\n");
}
