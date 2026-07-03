// TempoSource seam (LNK-008): one interface, two adapters (Link / USB MIDI),
// selected at runtime from the NVS input_source field. The app core (.ino)
// talks only to tempo_source_* and never touches link_listener / midi_* again.
#include "tempo_source.h"
#include "config.h"
#include "link_listener.h"
#include "link_measurement_io.h"
#include "link_protocol.h"
#include "link_measurement.h"
#include "link_phase.h"
#include "midi_clock.h"
#include "midi_bpm.h"
#include "midi_bpm_calc.h"
#include "app_config.h"
#include <USB.h>
#include <Arduino.h>
#include "esp_timer.h"  // esp_timer_get_time() — same clock as LNK-018's
                         // measurement path; see link_measurement_io.cpp

static int s_kind = TEMPO_SRC_LINK;

// quantum_beats lives on the shared AppConfig (X32Link.ino owns the
// instance) — read directly here rather than threading it through every
// call, same pattern as osc_sender.cpp/web_config.cpp.
extern AppConfig g_config;

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

// Link joins multicast AFTER WiFi is up. The measurement client (LNK-018)
// rides along on its own unicast socket — it doesn't expose phase yet
// (that's LNK-019), it just keeps a GhostXForm warm in the background once
// a peer is discovered.
extern "C" void tempo_source_begin(void) {
    if (s_kind == TEMPO_SRC_LINK) {
        link_listener_begin();
        link_measurement_io_begin();
    }
}

extern "C" void tempo_source_poll(void) {
    if (s_kind == TEMPO_SRC_LINK) {
        link_listener_poll();
        link_listener_tick();
        link_measurement_io_poll();
    }
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

// Phase within the current quantum (bar) — LNK-019. Link: maps our local
// esp_timer clock into the session's shared GHostTime domain via LNK-018's
// committed GhostXForm, reads LNK-017's parsed timeline, and runs the pure
// beats/phase math in link_phase.c. MIDI: pulse-count modulo pulses-per-bar
// via midi_bpm_calc.c's midi_phase_calc() — exact, not estimated, since
// MIDI clock pulses are directly observed.
extern "C" float tempo_source_phase(float quantum) {
    if (!tempo_source_phase_valid()) return -1.0f;

    if (s_kind == TEMPO_SRC_LINK) {
        LinkTimeline timeline;
        link_proto_timeline(&timeline);  // valid() already confirmed true
        LinkGhostXForm xform = link_measurement_current_xform();
        int64_t ghost_now_us = link_ghost_xform_host_to_ghost(xform, esp_timer_get_time());
        double beats_now = link_phase_beats_now(timeline, ghost_now_us);
        return (float)link_phase_from_beats(beats_now, (double)quantum);
    }

    return midi_phase_calc(midi_clock_pulse_count(), (int)quantum);
}

extern "C" bool tempo_source_phase_valid(void) {
    if (s_kind == TEMPO_SRC_LINK) {
        LinkTimeline timeline;
        // Valid once we hold a COMMITTED GhostXForm and the timeline is parsed.
        // NOT link_measurement_active(): `active` is true only while an attempt is
        // in flight and goes false the instant it commits, so gating on it made
        // phase valid for the ~ms an attempt ran and invalid the moment it
        // succeeded — the intermittent "flashing" phase. The committed xform (same
        // one tempo_source_phase() reads) is the real source of truth.
        return link_measurement_current_xform().valid && link_proto_timeline(&timeline);
    }
    return midi_phase_valid(midi_clock_pulse_count(), g_config.quantum_beats);
}

extern "C" float tempo_source_threshold(void) {
    return (s_kind == TEMPO_SRC_LINK) ? LINK_BPM_THRESHOLD : MCK_BPM_THRESHOLD;
}

extern "C" uint32_t tempo_source_poll_ms(void) {
    return (s_kind == TEMPO_SRC_LINK) ? LINK_POLL_MS : MCK_POLL_MS;
}
