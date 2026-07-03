#include "midi_clock.h"
#include "config.h"
#include <USBMIDI.h>
#include <Arduino.h>

#define RING_SIZE (MCK_CLOCK_WINDOW * 2)

USBMIDI MidiUSB("X32MidiClock");

// LNK-027: track whether the USBMIDI endpoint is already up, so the OUT path
// (Link mode) and the IN path (MIDI mode) can each call begin() without a
// double-begin.
static bool s_usb_begun = false;

static volatile uint32_t s_ring[RING_SIZE];
static volatile uint32_t s_head        = 0;
static volatile uint32_t s_pulse_count = 0;
static volatile bool     s_beat_flag   = false;
static portMUX_TYPE      s_mux         = portMUX_INITIALIZER_UNLOCKED;

// High-priority poll task — calls readPacket() at 1 ms intervals.
// MIDI Clock is a single-byte real-time message (0xF8), CIN=0x0F.
static void midi_poll_task(void*) {
    midiEventPacket_t pkt;
    for (;;) {
        while (MidiUSB.readPacket(&pkt)) {
            if (pkt.byte1 == 0xF8) {
                uint32_t t = micros();
                portENTER_CRITICAL(&s_mux);
                s_ring[s_head % RING_SIZE] = t;
                s_head++;
                s_pulse_count++;
                if (s_pulse_count % MCK_PPQN == 0) s_beat_flag = true;  // per beat, not per BPM window
                portEXIT_CRITICAL(&s_mux);
            }
        }
        vTaskDelay(1);  // 1 ms — fine at 240 BPM (pulse every ~10 ms)
    }
}

void midi_clock_init() {
    midi_clock_usb_begin();
    xTaskCreatePinnedToCore(midi_poll_task, "midi_poll", 2048, NULL, 5, NULL, 1);
}

// LNK-027 MIDI clock OUT helpers (shared USBMIDI endpoint).
void midi_clock_usb_begin() {
    if (s_usb_begun) return;
    MidiUSB.begin();
    s_usb_begun = true;
}

void midi_clock_send_f8() {
    // System real-time Timing Clock (0xF8): USB-MIDI CIN 0x0F, cable 0.
    midiEventPacket_t pkt = {0x0F, 0xF8, 0x00, 0x00};
    MidiUSB.writePacket(&pkt);
}

uint32_t midi_clock_pulse_count() {
    return s_pulse_count;
}

uint32_t midi_clock_last_pulse_us() {
    if (s_pulse_count == 0) return 0;
    portENTER_CRITICAL(&s_mux);
    uint32_t t = s_ring[(s_head - 1) % RING_SIZE];
    portEXIT_CRITICAL(&s_mux);
    return t;
}

bool midi_clock_beat_flag() {
    if (!s_beat_flag) return false;
    portENTER_CRITICAL(&s_mux);
    s_beat_flag = false;
    portEXIT_CRITICAL(&s_mux);
    return true;
}

// index 0 = oldest pulse in the current MCK_CLOCK_WINDOW window
uint32_t midi_clock_get_timestamp(uint32_t index) {
    portENTER_CRITICAL(&s_mux);
    uint32_t slot = (s_head - MCK_CLOCK_WINDOW + index) % RING_SIZE;
    uint32_t t    = s_ring[slot];
    portEXIT_CRITICAL(&s_mux);
    return t;
}
