#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t micros_per_beat;
    int64_t beat_origin_micro;
    int64_t time_origin_us;
} LinkTimeline;

void   link_proto_reset(void);
void   link_proto_tick(void);
bool   link_proto_parse(const uint8_t* buf, int len);
double link_proto_bpm(void);
int    link_proto_peers(void);
bool   link_proto_timeline(LinkTimeline* out);
bool   link_proto_peer_endpoint(int index, uint32_t* ip, uint16_t* port);

#ifdef __cplusplus
}
#endif
