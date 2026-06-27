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
    cfg->model        = DEFAULT_MODEL;
    cfg->fx_slot      = DEFAULT_FX_SLOT;
    cfg->input_source = DEFAULT_INPUT_SOURCE;
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
    return true;
}
