#include "touch_ui.h"
#include <stdio.h>
#include <string.h>

int ui_hit(const ui_rect_t *r, int n, int x, int y) {
    if (!r) return -1;
    for (int i = 0; i < n; i++) {
        if (x >= r[i].x && x < r[i].x + r[i].w &&
            y >= r[i].y && y < r[i].y + r[i].h) {
            return i;
        }
    }
    return -1;
}

void ui_bpm_str(char *out, size_t n, float bpm) {
    if (!out || n == 0) return;
    if (bpm <= 0.0f) {
        snprintf(out, n, "--.-");
    } else {
        snprintf(out, n, "%.1f", bpm);
    }
}
