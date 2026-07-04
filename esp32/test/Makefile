CC      = gcc
CFLAGS  = -Wall -Wextra -g \
           -I../X32Link \
           -I../X32FaderDisp \
           -I../lib/unity/src

UNITY   = ../lib/unity/src/unity.c
PROTO   = ../X32Link/link_protocol.c
MEAS    = ../X32Link/link_measurement.c
PHASE   = ../X32Link/link_phase.c
LEDPHASE = ../X32Link/led_phase.c
WIFIBLINK = ../X32Link/wifi_down_blink.c
TRACKER = ../X32Link/bpm_tracker.c
CFG     = ../X32Link/app_config.c
OSC     = ../X32Link/osc_out.c
MIDICALC = ../X32MidiClock/midi_bpm_calc.c
PUB     = ../X32Link/bpm_publisher.c
OSCIN   = ../X32Link/osc_in.c
FADERDB = ../X32FaderDisp/fader_db.c
STATUSJSON = ../X32Link/web_status_json.c
AXS     = ../X32Link/axs5106l.c
TOUCHUI = ../X32Link/touch_ui.c
SNAP    = ../X32Link/tempo_snapshot.c
BAR     = ../X32Link/bar.c
MIDIOUT = ../X32Link/midi_clock_out.c
CLOCKTICK = ../X32Link/clock_ticker.c
BEATSYNTH = ../X32Link/beat_synth.c
SESSION = ../X32Link/link_measurement_session.c

.PHONY: all clean

all: test_bpm_tracker test_link_protocol test_link_measurement test_link_phase test_led_phase test_wifi_down_blink test_app_config test_osc_out test_midi_bpm_calc test_bpm_publisher test_osc_in test_fader_db test_web_status_json test_touch_ui test_tempo_snapshot test_bar test_midi_clock_out test_clock_ticker test_beat_synth test_link_measurement_session
	./test_bpm_tracker
	./test_link_protocol
	./test_link_measurement
	./test_link_phase
	./test_led_phase
	./test_wifi_down_blink
	./test_app_config
	./test_osc_out
	./test_midi_bpm_calc
	./test_bpm_publisher
	./test_osc_in
	./test_fader_db
	./test_web_status_json
	./test_touch_ui
	./test_tempo_snapshot
	./test_bar
	./test_midi_clock_out
	./test_clock_ticker
	./test_beat_synth
	./test_link_measurement_session
	./check_docs.sh

test_bpm_tracker: test_bpm_tracker.c $(TRACKER) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_link_protocol: test_link_protocol.c $(PROTO) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_link_measurement: test_link_measurement.c $(MEAS) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_link_phase: test_link_phase.c $(PHASE) $(PROTO) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_led_phase: test_led_phase.c $(LEDPHASE) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_wifi_down_blink: test_wifi_down_blink.c $(WIFIBLINK) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_app_config: test_app_config.c $(CFG) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_osc_out: test_osc_out.c $(OSC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_midi_bpm_calc: test_midi_bpm_calc.c $(MIDICALC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_bpm_publisher: test_bpm_publisher.c $(PUB) $(BAR) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_osc_in: test_osc_in.c $(OSCIN) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_fader_db: test_fader_db.c $(FADERDB) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_web_status_json: test_web_status_json.c $(STATUSJSON) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_axs5106l: test_axs5106l.c $(AXS) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_touch_ui: test_touch_ui.c $(TOUCHUI) $(CFG) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_tempo_snapshot: test_tempo_snapshot.c $(SNAP) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_bar: test_bar.c $(BAR) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_midi_clock_out: test_midi_clock_out.c $(MIDIOUT) $(CLOCKTICK) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_clock_ticker: test_clock_ticker.c $(CLOCKTICK) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_beat_synth: test_beat_synth.c $(BEATSYNTH) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_link_measurement_session: test_link_measurement_session.c $(SESSION) $(PHASE) $(PROTO) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

clean:
	rm -f test_bpm_tracker test_link_protocol test_link_measurement test_link_phase test_led_phase test_wifi_down_blink test_app_config test_osc_out test_midi_bpm_calc test_bpm_publisher test_osc_in test_fader_db test_web_status_json test_axs5106l test_touch_ui test_tempo_snapshot test_bar
