#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_XR18  1
#define MODEL_X32   2

typedef struct {
    char wifi_ssid[64];
    char wifi_pass[64];
    char mixer_ip[16];
    int  model;         // MODEL_XR18 or MODEL_X32
    int  fx_slot;       // 1–4 (XR18) or 1–8 (X32)
    int  input_source;  // 0 = Ableton Link, 1 = USB MIDI clock
    int  fdr_enable;    // X32FaderDisp: 0 = off, 1 = write dB to scribble names
    int  fdr_chan_count;// X32FaderDisp: 16 (XR18) or 32 (X32)
    int  quantum_beats; // bar-quantized phase: beats per bar, 1-16 (LNK-019)
    int  midi_clock_out_enable; // LNK-027: 0/1 — emit 24-PPQN USB-MIDI clock while following Link
} AppConfig;

void config_defaults(AppConfig* cfg);
int  config_model_port(int model);
int  config_model_slot_max(int model);
bool config_validate(const AppConfig* cfg);

// NVS-backed persistence (implemented in app_config_nvs.cpp)
void config_load(AppConfig* cfg);
void config_save(const AppConfig* cfg);
void config_clear(void);

#ifdef __cplusplus
}
#endif
