#include "sip/sip.h"
#include <string.h>
#include <stdio.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <esp_system.h>

#define SIP_BUF_SIZE    2048
#define SIP_RECV_SIZE   4096
#define SIP_TIMEOUT_MS  5000
#define MD5_DIGEST_LEN  16
#define MD5_HEX_LEN     33

static const char hex_chars[] = "0123456789abcdef";

/* === MD5 implementation (RFC 1321) === */

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t  buffer[64];
} md5_ctx_t;

static const uint32_t md5_k[] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t md5_rounds[] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static uint32_t md5_f(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static uint32_t md5_g(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static uint32_t md5_h(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static uint32_t md5_i(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

static uint32_t md5_rotl(uint32_t x, uint8_t n) { return (x << n) | (x >> (32 - n)); }

static void md5_init(md5_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count = 0;
}

static void md5_block(md5_ctx_t *ctx, const uint8_t *block)
{
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t x[16];

    for (int i = 0; i < 16; i++) {
        x[i] = (uint32_t)block[i * 4]
             | ((uint32_t)block[i * 4 + 1] << 8)
             | ((uint32_t)block[i * 4 + 2] << 16)
             | ((uint32_t)block[i * 4 + 3] << 24);
    }

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = md5_f(b, c, d);
            g = i;
        } else if (i < 32) {
            f = md5_g(b, c, d);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = md5_h(b, c, d);
            g = (3 * i + 5) % 16;
        } else {
            f = md5_i(b, c, d);
            g = (7 * i) % 16;
        }

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + md5_rotl(a + f + md5_k[i] + x[g], md5_rounds[i]);
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

static void md5_update(md5_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t idx = (size_t)(ctx->count & 0x3f);
    ctx->count += len;

    if (idx) {
        size_t fill = 64 - idx;
        if (len < fill) {
            memcpy(ctx->buffer + idx, data, len);
            return;
        }
        memcpy(ctx->buffer + idx, data, fill);
        md5_block(ctx, ctx->buffer);
        data += fill;
        len -= fill;
    }

    while (len >= 64) {
        md5_block(ctx, data);
        data += 64;
        len -= 64;
    }

    if (len) {
        memcpy(ctx->buffer, data, len);
    }
}

static void md5_final(md5_ctx_t *ctx, uint8_t digest[MD5_DIGEST_LEN])
{
    uint64_t bits = ctx->count * 8;
    size_t idx = (size_t)(ctx->count & 0x3f);
    size_t pad = (idx < 56) ? (56 - idx) : (120 - idx);

    uint8_t padding[64];
    memset(padding, 0, pad);
    padding[0] = 0x80;

    md5_update(ctx, padding, pad);

    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++) {
        len_bytes[i] = (uint8_t)(bits >> (i * 8));
    }
    md5_update(ctx, len_bytes, 8);

    for (int i = 0; i < 4; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

static void md5_hex_string(const char *input, char output[MD5_HEX_LEN])
{
    md5_ctx_t ctx;
    uint8_t digest[MD5_DIGEST_LEN];

    md5_init(&ctx);
    md5_update(&ctx, (const uint8_t *)input, strlen(input));
    md5_final(&ctx, digest);

    for (int i = 0; i < MD5_DIGEST_LEN; i++) {
        output[i * 2]     = hex_chars[digest[i] >> 4];
        output[i * 2 + 1] = hex_chars[digest[i] & 0x0f];
    }
    output[32] = '\0';
}

/* === SIP helper functions === */

static int sip_send(sip_ctx_t *ctx, const char *buf, int len)
{
    return sendto(ctx->socket_fd, buf, len, 0,
                  (struct sockaddr *)&ctx->server_addr,
                  sizeof(ctx->server_addr));
}

static int sip_recv(int fd, char *buf, size_t buf_size, int *status_code)
{
    ssize_t n = recvfrom(fd, buf, buf_size - 1, 0, NULL, NULL);
    if (n <= 0) {
        return -1;
    }

    buf[n] = '\0';

    if (status_code) {
        if (sscanf(buf, "SIP/2.0 %3d", status_code) < 1) {
            *status_code = 0;
        }
    }

    return (int)n;
}

static char *sip_get_header(const char *msg, const char *hdr, char *val, size_t val_sz)
{
    const char *p = msg;
    size_t hdr_len = strlen(hdr);

    while (p) {
        p = strstr(p, hdr);
        if (!p) return NULL;

        const char *after = p + hdr_len;
        if (*after == ':') {
            const char *v = after + 1;
            while (*v == ' ' || *v == '\t') v++;
            const char *end = strstr(v, "\r\n");
            if (!end) end = v + strlen(v);
            size_t len = (size_t)(end - v);
            if (len >= val_sz) len = val_sz - 1;
            memcpy(val, v, len);
            val[len] = '\0';
            return val;
        }
        p = after;
    }

    return NULL;
}

static int sip_parse_auth(const char *msg, char *realm, size_t realm_sz,
                          char *nonce, size_t nonce_sz)
{
    // Search case-insensitively for WWW-Authenticate
    const char *p = msg;
    const char *auth_start = NULL;

    while (*p) {
        if (strncasecmp(p, "WWW-Authenticate:", 17) == 0) {
            auth_start = p + 17;
            break;
        }
        p++;
    }

    if (!auth_start) {
        printf("SIP: WWW-Authenticate header not found\n");
        return -1;
    }

    // Skip spaces
    while (*auth_start == ' ' || *auth_start == '\t') auth_start++;

    // Find end of header line
    char auth_hdr[512];
    const char *end = strstr(auth_start, "\r\n");
    if (!end) end = auth_start + strlen(auth_start);
    size_t len = (size_t)(end - auth_start);
    if (len >= sizeof(auth_hdr)) len = sizeof(auth_hdr) - 1;
    memcpy(auth_hdr, auth_start, len);
    auth_hdr[len] = '\0';

    printf("SIP: WWW-Authenticate: %s\n", auth_hdr);

    const char *r = strstr(auth_hdr, "realm=");
    if (r) {
        r += 6;
        if (*r == '"') r++;
        const char *re = strchr(r, '"');
        if (!re) re = r + strlen(r);
        size_t rlen = (size_t)(re - r);
        if (rlen >= realm_sz) rlen = realm_sz - 1;
        memcpy(realm, r, rlen);
        realm[rlen] = '\0';
    } else {
        printf("SIP: realm not found\n");
        return -1;
    }

    const char *n = strstr(auth_hdr, "nonce=");
    if (n) {
        n += 6;
        if (*n == '"') n++;
        const char *ne = strchr(n, '"');
        if (!ne) ne = n + strlen(n);
        size_t nlen = (size_t)(ne - n);
        if (nlen >= nonce_sz) nlen = nonce_sz - 1;
        memcpy(nonce, n, nlen);
        nonce[nlen] = '\0';
    } else {
        printf("SIP: nonce not found\n");
        return -1;
    }

    printf("SIP: realm='%s' nonce='%s'\n", realm, nonce);
    return 0;
}

static int sip_parse_sdp(const char *msg, char *rtp_ip, size_t ip_sz,
                         uint16_t *rtp_port)
{
    const char *c = strstr(msg, "c=IN IP4 ");
    if (c) {
        c += 9;
        const char *end = strchr(c, '\r');
        if (!end) end = strchr(c, '\n');
        if (!end) end = c + strlen(c);
        size_t len = (size_t)(end - c);
        if (len >= ip_sz) len = ip_sz - 1;
        memcpy(rtp_ip, c, len);
        rtp_ip[len] = '\0';
    } else {
        return -1;
    }

    const char *m = strstr(msg, "m=audio ");
    if (m) {
        m += 8;
        unsigned port = 0;
        while (*m >= '0' && *m <= '9') {
            port = port * 10 + (unsigned)(*m - '0');
            m++;
        }
        if (port > 0 && port <= 65535) {
            *rtp_port = (uint16_t)port;
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

/* === Main API functions === */

void sip_init(sip_ctx_t *ctx, const sip_config_t *config)
{
    memset(ctx, 0, sizeof(*ctx));
    memcpy(&ctx->config, config, sizeof(sip_config_t));

    ctx->state = SIP_STATE_IDLE;
    ctx->cseq = 1;

    ctx->call_id_num = esp_random();
    ctx->tag_local = esp_random();

    ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->socket_fd < 0) {
        printf("SIP: socket create failed\n");
        ctx->state = SIP_STATE_ERROR;
        return;
    }

    int opt = 1;
    setsockopt(ctx->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(ctx->config.local_port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctx->socket_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        printf("SIP: bind to port %d failed\n", ctx->config.local_port);
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        ctx->state = SIP_STATE_ERROR;
        return;
    }

    memset(&ctx->server_addr, 0, sizeof(ctx->server_addr));
    ctx->server_addr.sin_family = AF_INET;
    ctx->server_addr.sin_port = htons(ctx->config.server_port);

    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(ctx->config.server, NULL, &hints, &res) == 0 && res) {
        ctx->server_addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    } else {
        inet_aton(ctx->config.server, &ctx->server_addr.sin_addr);
    }

    printf("SIP: initialized, server=%s:%u\n",
           ctx->config.server, (unsigned)ctx->config.server_port);
}

int sip_register(sip_ctx_t *ctx)
{
    static char buf[SIP_BUF_SIZE];
    static char recv_buf[SIP_RECV_SIZE];
    int status = 0;
    int len;

    ctx->state = SIP_STATE_REGISTERING;

    /* === First REGISTER (no auth) === */

    len = snprintf(buf, sizeof(buf),
        "REGISTER sip:%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%u;branch=z9hG4bK%08lx\r\n"
        "From: <sip:%s@%s>;tag=%08lx\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08lx@%s\r\n"
        "CSeq: %u REGISTER\r\n"
        "Contact: <sip:%s@%s:%u>\r\n"
        "Max-Forwards: 70\r\n"
        "Expires: 3600\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        ctx->config.server,
        ctx->config.local_ip, (unsigned)ctx->config.local_port,
        (unsigned long)esp_random(),
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->tag_local,
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->call_id_num, ctx->config.local_ip,
        (unsigned)ctx->cseq,
        ctx->config.username, ctx->config.local_ip,
        (unsigned)ctx->config.local_port);

    if (sip_send(ctx, buf, len) < 0) {
        printf("SIP: REGISTER send failed\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    ctx->cseq++;

    if (sip_recv(ctx->socket_fd, recv_buf, sizeof(recv_buf), &status) < 0) {
        printf("SIP: REGISTER response timeout\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    if (status == 200) {
        ctx->state = SIP_STATE_REGISTERED;
        printf("SIP: registered successfully\n");
        return 0;
    }

    if (status != 401) {
        printf("SIP: REGISTER unexpected status %d\n", status);
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    /* === 401 Unauthorized — digest auth === */

    char realm[128], nonce[128];
    if (sip_parse_auth(recv_buf, realm, sizeof(realm), nonce, sizeof(nonce)) < 0) {
        printf("SIP: failed to parse WWW-Authenticate\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    char ha1[MD5_HEX_LEN], ha2[MD5_HEX_LEN], response[MD5_HEX_LEN];
    char input[512];

    snprintf(input, sizeof(input), "%s:%s:%s",
             ctx->config.username, realm, ctx->config.password);
    md5_hex_string(input, ha1);

    snprintf(input, sizeof(input), "REGISTER:sip:%s", ctx->config.server);
    md5_hex_string(input, ha2);

    snprintf(input, sizeof(input), "%s:%s:%s", ha1, nonce, ha2);
    md5_hex_string(input, response);

    len = snprintf(buf, sizeof(buf),
        "REGISTER sip:%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%u;branch=z9hG4bK%08lx\r\n"
        "From: <sip:%s@%s>;tag=%08lx\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08lx@%s\r\n"
        "CSeq: %u REGISTER\r\n"
        "Contact: <sip:%s@%s:%u>\r\n"
        "Authorization: Digest username=\"%s\", realm=\"%s\", "
            "nonce=\"%s\", uri=\"sip:%s\", response=\"%s\"\r\n"
        "Max-Forwards: 70\r\n"
        "Expires: 3600\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        ctx->config.server,
        ctx->config.local_ip, (unsigned)ctx->config.local_port,
        (unsigned long)esp_random(),
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->tag_local,
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->call_id_num, ctx->config.local_ip,
        (unsigned)ctx->cseq,
        ctx->config.username, ctx->config.local_ip,
        (unsigned)ctx->config.local_port,
        ctx->config.username, realm,
        nonce, ctx->config.server, response);

    if (sip_send(ctx, buf, len) < 0) {
        printf("SIP: REGISTER (auth) send failed\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    ctx->cseq++;

    if (sip_recv(ctx->socket_fd, recv_buf, sizeof(recv_buf), &status) < 0) {
        printf("SIP: REGISTER (auth) response timeout\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    if (status == 200) {
        ctx->state = SIP_STATE_REGISTERED;
        printf("SIP: registered successfully (with auth)\n");
        return 0;
    }

    printf("SIP: REGISTER (auth) got status %d\n", status);
    ctx->state = SIP_STATE_ERROR;
    return -1;
}

int sip_call(sip_ctx_t *ctx, const char *number)
{
    static char buf[SIP_BUF_SIZE];
    static char recv_buf[SIP_RECV_SIZE];
    static char sdp[512];
    int status;
    int len;

    if (ctx->state != SIP_STATE_REGISTERED) {
        printf("SIP: not registered\n");
        return -1;
    }

    ctx->state = SIP_STATE_CALLING;

    int sdp_len = snprintf(sdp, sizeof(sdp),
        "v=0\r\n"
        "o=- 0 0 IN IP4 %s\r\n"
        "s=EspCall\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio %u RTP/AVP 0\r\n"
        "a=rtpmap:0 PCMU/8000\r\n",
        ctx->config.local_ip, ctx->config.local_ip,
        (unsigned)ctx->config.rtp_port);

    len = snprintf(buf, sizeof(buf),
        "INVITE sip:%s@%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%u;branch=z9hG4bK%08lx\r\n"
        "From: <sip:%s@%s>;tag=%08lx\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08lx@%s\r\n"
        "CSeq: %u INVITE\r\n"
        "Contact: <sip:%s@%s:%u>\r\n"
        "Max-Forwards: 70\r\n"
        "Content-Type: application/sdp\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        number, ctx->config.server,
        ctx->config.local_ip, (unsigned)ctx->config.local_port,
        (unsigned long)esp_random(),
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->tag_local,
        number, ctx->config.server,
        (unsigned long)ctx->call_id_num, ctx->config.local_ip,
        (unsigned)ctx->cseq,
        ctx->config.username, ctx->config.local_ip,
        (unsigned)ctx->config.local_port,
        sdp_len, sdp);

    if (sip_send(ctx, buf, len) < 0) {
        printf("SIP: INVITE send failed\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    uint32_t invite_cseq = ctx->cseq;
    ctx->cseq++;

    /* === Wait for final response === */

    while (1) {
        if (sip_recv(ctx->socket_fd, recv_buf, sizeof(recv_buf), &status) < 0) {
            printf("SIP: INVITE response timeout\n");
            ctx->state = SIP_STATE_ERROR;
            return -1;
        }

        if (status == 100) {
            printf("SIP: Trying...\n");
            continue;
        }

        if (status == 180) {
            printf("SIP: Ringing...\n");
            continue;
        }

        if (status == 200) {
            printf("SIP: 200 OK for INVITE\n");

            if (sip_parse_sdp(recv_buf, ctx->remote_rtp_ip,
                              sizeof(ctx->remote_rtp_ip),
                              &ctx->remote_rtp_port) < 0) {
                printf("SIP: failed to parse SDP\n");
                ctx->state = SIP_STATE_ERROR;
                return -1;
            }

            /* === Send ACK === */
            len = snprintf(buf, sizeof(buf),
                "ACK sip:%s@%s SIP/2.0\r\n"
                "Via: SIP/2.0/UDP %s:%u;branch=z9hG4bK%08lx\r\n"
                "From: <sip:%s@%s>;tag=%08lx\r\n"
                "To: <sip:%s@%s>\r\n"
                "Call-ID: %08lx@%s\r\n"
                "CSeq: %u ACK\r\n"
                "Max-Forwards: 70\r\n"
                "Content-Length: 0\r\n"
                "\r\n",
                number, ctx->config.server,
                ctx->config.local_ip, (unsigned)ctx->config.local_port,
                (unsigned long)esp_random(),
                ctx->config.username, ctx->config.server,
                (unsigned long)ctx->tag_local,
                number, ctx->config.server,
                (unsigned long)ctx->call_id_num, ctx->config.local_ip,
                (unsigned)invite_cseq);

            sip_send(ctx, buf, len);

            ctx->state = SIP_STATE_IN_CALL;
            printf("SIP: call established, RTP %s:%u\n",
                   ctx->remote_rtp_ip, (unsigned)ctx->remote_rtp_port);
            return 0;
        }

        printf("SIP: INVITE unexpected status %d\n", status);
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }
}

int sip_bye(sip_ctx_t *ctx)
{
    static char buf[SIP_BUF_SIZE];
    static char recv_buf[SIP_RECV_SIZE];
    int status;

    if (ctx->state != SIP_STATE_IN_CALL) {
        return -1;
    }

    ctx->state = SIP_STATE_HANGING_UP;

    int len = snprintf(buf, sizeof(buf),
        "BYE sip:%s SIP/2.0\r\n"
        "Via: SIP/2.0/UDP %s:%u;branch=z9hG4bK%08lx\r\n"
        "From: <sip:%s@%s>;tag=%08lx\r\n"
        "To: <sip:%s@%s>\r\n"
        "Call-ID: %08lx@%s\r\n"
        "CSeq: %u BYE\r\n"
        "Max-Forwards: 70\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        ctx->config.server,
        ctx->config.local_ip, (unsigned)ctx->config.local_port,
        (unsigned long)esp_random(),
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->tag_local,
        ctx->config.username, ctx->config.server,
        (unsigned long)ctx->call_id_num, ctx->config.local_ip,
        (unsigned)ctx->cseq);

    if (sip_send(ctx, buf, len) < 0) {
        printf("SIP: BYE send failed\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    ctx->cseq++;

    if (sip_recv(ctx->socket_fd, recv_buf, sizeof(recv_buf), &status) < 0) {
        printf("SIP: BYE response timeout\n");
        ctx->state = SIP_STATE_ERROR;
        return -1;
    }

    if (status == 200) {
        ctx->state = SIP_STATE_IDLE;
        printf("SIP: call hung up\n");
        return 0;
    }

    printf("SIP: BYE unexpected status %d\n", status);
    ctx->state = SIP_STATE_ERROR;
    return -1;
}

void sip_close(sip_ctx_t *ctx)
{
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    ctx->state = SIP_STATE_IDLE;
    printf("SIP: closed\n");
}
