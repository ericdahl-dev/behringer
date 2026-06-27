#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void   link_proto_reset(void);
void   link_proto_tick(void);
bool   link_proto_parse(const uint8_t* buf, int len);
double link_proto_bpm(void);
int    link_proto_peers(void);

#ifdef __cplusplus
}
#endif
