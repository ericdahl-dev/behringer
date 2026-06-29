#ifndef OSC_IO_H
#define OSC_IO_H

#include <stdint.h>
#include <netinet/in.h>

typedef struct {
    int                fd;
    struct sockaddr_in xip;
    struct sockaddr   *xip_addr;
    socklen_t          xip_len;
} OscConn;

/* Lifecycle */
int  osc_conn_init(OscConn *c, const char *ip, int port);
void osc_conn_close(OscConn *c);

/* /xinfo handshake — returns 0 on success, -1 on timeout/mismatch */
int osc_handshake(OscConn *c);

/* Fire-and-forget sends */
void osc_send_no_args(OscConn *c, const char *path);
void osc_send_int(OscConn *c, const char *path, int val);
void osc_send_float(OscConn *c, const char *path, float val);
void osc_send_int_float(OscConn *c, const char *path, int ival, float fval);

/* Request-reply queries — return 0 and set *out, or -1 on timeout/error */
int osc_query_int(OscConn *c, const char *path, int *out);
int osc_query_float(OscConn *c, const char *path, float *out);

/* Locate the blob payload in a received OSC packet.
 * buf/buflen: raw UDP bytes (buf must be null-terminated within buflen).
 * Returns pointer to first blob byte and sets *blob_len (= buflen - blob_offset),
 * or NULL if blob_start >= buflen or buffer is too short to parse. */
const uint8_t *osc_locate_blob(const char *buf, int buflen, int *blob_len);

/* X32/XR18 /node query — sends "/node ,s <node>" and copies the reply string
 * into out[0..outsz-1]. Returns 0 on success, -1 on timeout/malformed reply. */
int osc_query_node(OscConn *c, const char *node, char *out, int outsz);

/* Subscribe to /meters/4 (RTA) pushes from the mixer. */
void osc_subscribe_meters4(OscConn *c);

#endif /* OSC_IO_H */
