#include "app_config.h"
#include <Preferences.h>

static Preferences prefs;

extern "C" void config_load(AppConfig* cfg) {
    config_defaults(cfg);
    prefs.begin("x32link", true);  // read-only
    cfg->model        = prefs.getInt("model",      cfg->model);
    cfg->fx_slot      = prefs.getInt("fx_slot",    cfg->fx_slot);
    cfg->input_source   = prefs.getInt("input_src", cfg->input_source);
    cfg->fdr_enable     = prefs.getInt("fdr_en",    cfg->fdr_enable);
    cfg->fdr_chan_count = prefs.getInt("fdr_ch",    cfg->fdr_chan_count);
    cfg->quantum_beats  = prefs.getInt("quantum_beats", cfg->quantum_beats);
    cfg->midi_clock_out_enable = prefs.getInt("mck_out", cfg->midi_clock_out_enable);
    prefs.getString("mixer_ip",  cfg->mixer_ip,  sizeof(cfg->mixer_ip));
    prefs.getString("wifi_ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
    prefs.getString("wifi_pass", cfg->wifi_pass, sizeof(cfg->wifi_pass));
    prefs.end();
}

extern "C" void config_save(const AppConfig* cfg) {
    prefs.begin("x32link", false);  // read-write
    prefs.putInt("model",      cfg->model);
    prefs.putInt("fx_slot",    cfg->fx_slot);
    prefs.putInt("input_src",  cfg->input_source);
    prefs.putInt("fdr_en",     cfg->fdr_enable);
    prefs.putInt("fdr_ch",     cfg->fdr_chan_count);
    prefs.putInt("quantum_beats", cfg->quantum_beats);
    prefs.putInt("mck_out",    cfg->midi_clock_out_enable);
    prefs.putString("mixer_ip",  cfg->mixer_ip);
    prefs.putString("wifi_ssid", cfg->wifi_ssid);
    prefs.putString("wifi_pass", cfg->wifi_pass);
    prefs.end();
}

extern "C" void config_clear(void) {
    prefs.begin("x32link", false);
    prefs.clear();
    prefs.end();
}
