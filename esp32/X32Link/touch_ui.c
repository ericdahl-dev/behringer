#include "touch_ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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

float ui_phase_angle(float phase, float quantum) {
    if (quantum <= 0.0f) return 0.0f;
    float frac = fmodf(phase, quantum);
    if (frac < 0.0f) frac += quantum;
    return frac / quantum * 360.0f;
}

void ui_apply_settings_tap(AppConfig *cfg, int field_id) {
    if (!cfg) return;
    switch (field_id) {
        case UI_F_SRC_LINK:
            cfg->input_source = 0;
            break;
        case UI_F_SRC_MIDI:
            cfg->input_source = 1;
            break;
        case UI_F_MODEL_XR:
        case UI_F_MODEL_X32: {
            cfg->model = (field_id == UI_F_MODEL_X32) ? MODEL_X32 : MODEL_XR18;
            int max = config_model_slot_max(cfg->model);
            if (cfg->fx_slot > max) cfg->fx_slot = max;
            break;
        }
        case UI_F_QUANTUM_INC:
            if (cfg->quantum_beats < 16) cfg->quantum_beats++;
            break;
        case UI_F_QUANTUM_DEC:
            if (cfg->quantum_beats > 1) cfg->quantum_beats--;
            break;
        default:
            if (field_id >= UI_F_SLOT_1 && field_id <= UI_F_SLOT_8) {
                int slot = field_id - UI_F_SLOT_1 + 1;
                if (slot <= config_model_slot_max(cfg->model)) {
                    cfg->fx_slot = slot;
                }
            }
            break;
    }
}
