#include "touch_ui.h"

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
