#include "osc_in.h"
#include <string.h>

static int32_t be32(const uint8_t *p) {
    return ((int32_t)p[0] << 24) | ((int32_t)p[1] << 16) |
           ((int32_t)p[2] << 8)  | (int32_t)p[3];
}

/* Offset past a NUL-terminated, 4-byte-aligned OSC string starting at `off`;
 * -1 if no terminator within `len`. */
static int skip_str(const uint8_t *buf, int len, int off) {
    int i = off;
    while (i < len && buf[i] != 0) i++;
    if (i >= len) return -1;          /* no NUL terminator */
    i = (i + 1 + 3) & ~3;             /* past NUL, then 4-align */
    return i;
}

int osc_in_parse(const uint8_t *buf, int len, osc_msg_t *msg) {
    msg->address = 0;
    msg->tags    = 0;
    msg->n_args  = 0;
    if (len < 4 || buf[0] != '/') return -1;

    msg->address = (const char *)buf;
    int off = skip_str(buf, len, 0);
    if (off < 0) return -1;

    if (off >= len || buf[off] != ',') return -1;   /* type-tag starts with ',' */
    msg->tags = (const char *)(buf + off + 1);
    int tag_start = off;
    off = skip_str(buf, len, tag_start);
    if (off < 0) return -1;

    for (const char *t = msg->tags; *t; t++) {
        if (msg->n_args >= OSC_IN_MAX_ARGS) break;
        osc_arg_t *a = &msg->args[msg->n_args];
        if (*t == 'i') {
            if (off + 4 > len) return -1;
            a->type = OSC_ARG_INT;
            a->val.i = be32(buf + off);
            off += 4;
        } else if (*t == 'f') {
            if (off + 4 > len) return -1;
            int32_t bits = be32(buf + off);
            float f;
            memcpy(&f, &bits, 4);
            a->type = OSC_ARG_FLOAT;
            a->val.f = f;
            off += 4;
        } else if (*t == 's') {
            if (off >= len) return -1;
            a->type = OSC_ARG_STRING;
            a->val.s = (const char *)(buf + off);
            off = skip_str(buf, len, off);
            if (off < 0) return -1;
        } else {
            break;   /* unknown tag (e.g. blob) — stop, keep args parsed so far */
        }
        msg->n_args++;
    }
    return 0;
}
