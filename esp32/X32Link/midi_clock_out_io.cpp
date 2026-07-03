// LNK-027 glue: drive USB-MIDI clock OUT from the Link phase pipeline. See
// midi_clock_out_io.h. Timing math is in the pure, host-tested midi_clock_out.c;
// this file only owns the FreeRTOS task and the TinyUSB writes.
#include "midi_clock_out_io.h"
#include "midi_clock_out.h"
#include "midi_clock.h"      // midi_clock_send_f8()
#include "tempo_source.h"    // tempo_source_beats_now()
#include <Arduino.h>

// A gap of more than this many 1/24-beat ticks between task ticks (a re-origin
// or a long stall) triggers a realign instead of a catch-up burst. At the 1ms
// task cadence, a normal step is 0-1 ticks even at high tempo, so 4 is generous
// slack for scheduling hiccups while still catching a real discontinuity.
static const int MAX_BURST = 4;

static MidiClockOut s_sched;

// 1ms high-priority task: quantize the current Link beat position to 24 PPQN and
// emit any due 0xF8 ticks. Resets the scheduler whenever phase isn't valid
// (pre-sync, peer loss, mid re-measure) so we never spray ticks off a stale or
// absent timeline, and re-prime cleanly when it comes back.
static void midi_clock_out_task(void*) {
    midi_clock_out_reset(&s_sched);
    for (;;) {
        double beats = tempo_source_beats_now();   // <0 when phase not valid
        if (beats < 0.0) {
            midi_clock_out_reset(&s_sched);
        } else {
            int n = midi_clock_out_ticks_due(&s_sched, beats, MAX_BURST);
            for (int i = 0; i < n; i++) midi_clock_send_f8();
        }
        vTaskDelay(1);  // 1 ms — tick period is ~10ms+ even at 240 BPM
    }
}

void midi_clock_out_io_begin(void) {
    midi_clock_usb_begin();  // idempotent; USB.begin() is done in tempo_source_pre_net
    xTaskCreatePinnedToCore(midi_clock_out_task, "midi_clk_out", 2048, NULL, 6, NULL, 1);
}
