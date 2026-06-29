#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSC_IN_MAX_ARGS 8

typedef enum {
    OSC_ARG_NONE = 0,
    OSC_ARG_INT,      /* 'i' — int32 */
    OSC_ARG_FLOAT,    /* 'f' — float32 */
    OSC_ARG_STRING,   /* 's' — string */
} osc_arg_type_t;

typedef struct {
    osc_arg_type_t type;
    union {
        int32_t     i;
        float       f;
        const char *s;   /* points into the parsed buffer */
    } val;
} osc_arg_t;

typedef struct {
    const char *address;            /* points into the parsed buffer */
    const char *tags;               /* type-tag string sans ',' */
    int         n_args;
    osc_arg_t   args[OSC_IN_MAX_ARGS];
} osc_msg_t;

/* Parse one OSC packet (big-endian, 4-byte-aligned). Pointers in `msg` reference
 * `buf`, which must outlive `msg`. Allocation-free. Returns 0 on success, -1 on
 * a malformed/oversized packet. Unknown type tags stop parsing (args up to that
 * point are kept). */
int osc_in_parse(const uint8_t *buf, int len, osc_msg_t *msg);

#ifdef __cplusplus
}
#endif
