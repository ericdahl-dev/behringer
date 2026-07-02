#include "app_config.h"
#include "config.h"
#include <string.h>

void config_defaults(AppConfig* cfg) {
    strncpy(cfg->wifi_ssid, DEFAULT_WIFI_SSID, sizeof(cfg->wifi_ssid) - 1);
    cfg->wifi_ssid[sizeof(cfg->wifi_ssid) - 1] = '\0';
    strncpy(cfg->wifi_pass, DEFAULT_WIFI_PASS, sizeof(cfg->wifi_pass) - 1);
    cfg->wifi_pass[sizeof(cfg->wifi_pass) - 1] = '\0';
    strncpy(cfg->mixer_ip,  DEFAULT_MIXER_IP,  sizeof(cfg->mixer_ip)  - 1);
    cfg->mixer_ip[sizeof(cfg->mixer_ip) - 1] = '\0';
    cfg->model          = DEFAULT_MODEL;
    cfg->fx_slot        = DEFAULT_FX_SLOT;
    cfg->input_source   = DEFAULT_INPUT_SOURCE;
    cfg->fdr_enable     = DEFAULT_FDR_ENABLE;
    cfg->fdr_chan_count = DEFAULT_FDR_CHAN_COUNT;
    cfg->quantum_beats  = DEFAULT_QUANTUM_BEATS;
}

int config_model_port(int model) {
    return model == MODEL_X32 ? 10023 : 10024;
}

int config_model_slot_max(int model) {
    return model == MODEL_X32 ? 8 : 4;
}

bool config_validate(const AppConfig* cfg) {
    if (cfg->mixer_ip[0] == '\0') return false;
    if (cfg->fx_slot < 1)                              return false;
    if (cfg->fx_slot > config_model_slot_max(cfg->model)) return false;
    if (cfg->input_source != 0 && cfg->input_source != 1)  return false;
    if (cfg->fdr_enable   != 0 && cfg->fdr_enable   != 1)  return false;
    if (cfg->fdr_chan_count != 16 && cfg->fdr_chan_count != 32) return false;
    if (cfg->quantum_beats < 1 || cfg->quantum_beats > 16)        return false;
    return true;
}
