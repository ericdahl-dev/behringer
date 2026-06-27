// TempoSource seam (LNK-008): one interface, two adapters (Link / USB MIDI),
// selected at runtime from the NVS input_source field. The app core (.ino)
// talks only to tempo_source_* and never touches link_listener / midi_* again.
#include "tempo_source.h"
#include "config.h"
#include "link_listener.h"
#include "midi_clock.h"
#include "midi_bpm.h"
#include <USB.h>
#include <Arduino.h>

static int s_kind = TEMPO_SRC_LINK;

extern "C" void tempo_source_select(int kind) {
    s_kind = (kind == TEMPO_SRC_MIDI) ? TEMPO_SRC_MIDI : TEMPO_SRC_LINK;
}

// USB MIDI must enumerate BEFORE WiFi. Link has nothing to do here.
extern "C" void tempo_source_pre_net(void) {
    if (s_kind == TEMPO_SRC_MIDI) {
        USB.manufacturerName("X32Sync");
        midi_clock_init();   // MidiUSB.begin() + spawns the poll task
        USB.begin();
        midi_bpm_init();
    }
}

// Link joins multicast AFTER WiFi is up.
extern "C" void tempo_source_begin(void) {
    if (s_kind == TEMPO_SRC_LINK) link_listener_begin();
}

extern "C" void tempo_source_poll(void) {
    if (s_kind == TEMPO_SRC_LINK) { link_listener_poll(); link_listener_tick(); }
    // MIDI is interrupt/task-driven (midi_poll_task) — nothing to pump here.
}

extern "C" float tempo_source_bpm(void) {
    return (s_kind == TEMPO_SRC_LINK) ? (float)link_listener_bpm()
                                      : midi_bpm_update();
}

extern "C" bool tempo_source_active(void) {
    return (s_kind == TEMPO_SRC_LINK) ? link_listener_peers() > 0
                                      : midi_bpm_update() > 0.0f;
}

// LED beat: MIDI forwards the clock's beat flag; Link synthesises from BPM.
extern "C" bool tempo_source_beat(void) {
    if (s_kind == TEMPO_SRC_MIDI) return midi_clock_beat_flag();
    float bpm = (float)link_listener_bpm();
    if (bpm <= 0.0f || link_listener_peers() == 0) return false;
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last >= (uint32_t)(60000.0f / bpm)) { last = now; return true; }
    return false;
}

extern "C" float tempo_source_threshold(void) {
    return (s_kind == TEMPO_SRC_LINK) ? LINK_BPM_THRESHOLD : MCK_BPM_THRESHOLD;
}

extern "C" uint32_t tempo_source_poll_ms(void) {
    return (s_kind == TEMPO_SRC_LINK) ? LINK_POLL_MS : MCK_POLL_MS;
}
