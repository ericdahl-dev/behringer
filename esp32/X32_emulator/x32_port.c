#include <string.h>
#include "x32_port.h"

extern char Xip_str[32];
extern char Xport_str[8];
extern int  Xdebug;
extern int  Xverbose;

void x32_get_ip(const char *wifi_ip) {
    if (!wifi_ip) return;
    strncpy(Xip_str, wifi_ip, 31);
    Xip_str[31] = 0;
}

void x32_apply_defaults(void) {
    strcpy(Xport_str, "10023");
    Xverbose = 1;
    Xdebug   = 0;
}

#ifndef ARDUINO
// Native stub — Arduino version uses lwIP socket directly
int x32_bind_socket(void) { return -1; }
#endif
