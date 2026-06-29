#include "osc_io.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdint.h>

extern int Xsprint(char *bd, int index, char format, void *bs);

#define OSC_BSIZE 512

static void osc_raw(OscConn *c, const char *buf, int len) {
    sendto(c->fd, buf, len, 0, c->xip_addr, c->xip_len);
}

int osc_conn_init(OscConn *c, const char *ip, int port) {
    c->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c->fd < 0) return -1;
    memset(&c->xip, 0, sizeof(c->xip));
    c->xip.sin_family      = AF_INET;
    c->xip.sin_addr.s_addr = inet_addr(ip);
    c->xip.sin_port        = htons((uint16_t)port);
    c->xip_addr = (struct sockaddr *)&c->xip;
    c->xip_len  = sizeof(c->xip);
    return 0;
}

void osc_conn_close(OscConn *c) {
    if (c->fd >= 0) { close(c->fd); c->fd = -1; }
}

int osc_handshake(OscConn *c) {
    char buf[OSC_BSIZE];
    int  len = Xsprint(buf, 0, 's', "/xinfo");
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(c->fd, &fds);
    sendto(c->fd, buf, len, 0, c->xip_addr, c->xip_len);
    if (select(c->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    if (recvfrom(c->fd, buf, OSC_BSIZE, 0, 0, 0) < 0) return -1;
    return strcmp(buf, "/xinfo") == 0 ? 0 : -1;
}

void osc_send_no_args(OscConn *c, const char *path) {
    char buf[OSC_BSIZE];
    int len = Xsprint(buf, 0, 's', (void *)path);
    osc_raw(c, buf, len);
}

void osc_send_int(OscConn *c, const char *path, int val) {
    char buf[OSC_BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",i");
    len = Xsprint(buf, len, 'i', &val);
    osc_raw(c, buf, len);
}

void osc_send_float(OscConn *c, const char *path, float val) {
    char buf[OSC_BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",f");
    len = Xsprint(buf, len, 'f', &val);
    osc_raw(c, buf, len);
}

void osc_send_int_float(OscConn *c, const char *path, int ival, float fval) {
    char buf[OSC_BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",if");
    len = Xsprint(buf, len, 'i', &ival);
    len = Xsprint(buf, len, 'f', &fval);
    osc_raw(c, buf, len);
}

static int osc_query_raw(OscConn *c, const char *path,
                          uint32_t *out_be, const char *expect_tag) {
    char buf[OSC_BSIZE];
    int  len = Xsprint(buf, 0, 's', (void *)path);
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(c->fd, &fds);
    sendto(c->fd, buf, len, 0, c->xip_addr, c->xip_len);
    if (select(c->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    int r = recvfrom(c->fd, buf, OSC_BSIZE - 1, 0, 0, 0);
    if (r <= 0) return -1;
    buf[r] = 0;
    if (strcmp(buf, path) != 0) return -1;
    int blob_len;
    const uint8_t *payload = osc_locate_blob(buf, r, &blob_len);
    if (!payload || blob_len < 4) return -1;
    (void)expect_tag;
    memcpy(out_be, payload, 4);
    return 0;
}

int osc_query_float(OscConn *c, const char *path, float *out) {
    uint32_t be;
    if (osc_query_raw(c, path, &be, ",f") != 0) return -1;
    be = ntohl(be);
    memcpy(out, &be, 4);
    return 0;
}

int osc_query_int(OscConn *c, const char *path, int *out) {
    uint32_t be;
    if (osc_query_raw(c, path, &be, ",i") != 0) return -1;
    *out = (int)ntohl(be);
    return 0;
}

const uint8_t *osc_locate_blob(const char *buf, int buflen, int *blob_len) {
    if (buflen < 1) return NULL;
    int ap = ((int)strlen(buf) + 1 + 3) & ~3;
    if (ap >= buflen) return NULL;
    int tp = ((int)strlen(buf + ap) + 1 + 3) & ~3;
    int blob_start = ap + tp;
    if (blob_start >= buflen) return NULL;
    if (blob_len) *blob_len = buflen - blob_start;
    return (const uint8_t *)buf + blob_start;
}

int osc_query_node(OscConn *c, const char *node, char *out, int outsz) {
    char buf[OSC_BSIZE];
    int  len = 0;
    len = Xsprint(buf, len, 's', "/node");
    len = Xsprint(buf, len, 's', ",s");
    len = Xsprint(buf, len, 's', (void *)node);
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(c->fd, &fds);
    sendto(c->fd, buf, len, 0, c->xip_addr, c->xip_len);
    if (select(c->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    int r = recvfrom(c->fd, buf, OSC_BSIZE - 1, 0, 0, 0);
    if (r <= 0) return -1;
    buf[r] = 0;
    if (strncmp(buf, "node", 4) != 0) return -1;
    int blob_len;
    const uint8_t *payload = osc_locate_blob(buf, r, &blob_len);
    if (!payload || blob_len < 1) return -1;
    strncpy(out, (const char *)payload, outsz - 1);
    out[outsz - 1] = 0;
    return 0;
}

void osc_subscribe_meters4(OscConn *c) {
    char buf[OSC_BSIZE];
    int zero = 0;
    int len = 0;
    len = Xsprint(buf, len, 's', "/meters");
    len = Xsprint(buf, len, 's', ",siii");
    len = Xsprint(buf, len, 's', "/meters/4");
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    sendto(c->fd, buf, len, 0, c->xip_addr, c->xip_len);
}
