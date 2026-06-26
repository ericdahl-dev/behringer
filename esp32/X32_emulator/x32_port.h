#ifndef X32_PORT_H
#define X32_PORT_H

// Platform seam: inject WiFi IP -> Xip_str (replaces getifaddrs on ESP32)
void x32_get_ip(const char *wifi_ip);

// Apply hardcoded defaults (replaces getopt)
void x32_apply_defaults(void);

// Create + bind UDP socket on Xip_str:Xport_str. Returns fd or -1.
int x32_bind_socket(void);

// State file path
#ifdef ARDUINO
  #define X32_STATE_FILE "/littlefs/.X32res.rc"
#else
  #define X32_STATE_FILE ".X32res.rc"
#endif

#endif
