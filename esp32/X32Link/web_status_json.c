// See web_status_json.h.
#include "web_status_json.h"
#include <stdio.h>

int web_status_json(char* buf, size_t buf_len, float bpm, float phase, bool valid, int quantum) {
    return snprintf(buf, buf_len,
        "{\"bpm\":%.1f,\"phase\":%.4f,\"valid\":%s,\"quantum\":%d}",
        (double)bpm, (double)phase, valid ? "true" : "false", quantum);
}
