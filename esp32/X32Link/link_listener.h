#pragma once
#include <stdint.h>
#include <stdbool.h>

void   link_listener_begin();
void   link_listener_poll();
void   link_listener_tick();
double link_listener_bpm();
int    link_listener_peers();
uint32_t link_listener_rx_count();
bool     link_listener_mcast_ok();
