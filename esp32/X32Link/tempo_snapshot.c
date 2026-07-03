#include "tempo_snapshot.h"

// Lock: on ESP32/Arduino a portMUX spinlock — brief, safe from any task, and
// what protects the publish/read from tearing across a context switch. On the
// host test build there is no concurrency, so it compiles to nothing. Same
// pure-core / thin-glue split the rest of the firmware uses (see AGENTS.md),
// which keeps this module host-testable.
#if defined(ARDUINO) || defined(ESP_PLATFORM)
  #include "freertos/FreeRTOS.h"
  static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
  #define TS_ENTER() portENTER_CRITICAL(&s_mux)
  #define TS_EXIT()  portEXIT_CRITICAL(&s_mux)
#else
  #define TS_ENTER() ((void)0)
  #define TS_EXIT()  ((void)0)
#endif

static TempoSnapshot s_snap = { 0.0f, -1.0f, false, 4 };

void tempo_snapshot_publish(float bpm, float phase, bool valid, int quantum) {
    bool coherent = valid && phase >= 0.0f;   // never valid with a sentinel phase
    TS_ENTER();
    s_snap.bpm     = bpm;
    s_snap.phase   = coherent ? phase : -1.0f;
    s_snap.valid   = coherent;
    s_snap.quantum = quantum;
    TS_EXIT();
}

void tempo_snapshot_read(TempoSnapshot* out) {
    if (!out) return;
    TS_ENTER();
    *out = s_snap;
    TS_EXIT();
}
