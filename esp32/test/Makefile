CC      = gcc
CFLAGS  = -Wall -Wextra -g \
           -I../X32Link \
           -I../X32FaderDisp \
           -I../lib/unity/src

UNITY   = ../lib/unity/src/unity.c
PROTO   = ../X32Link/link_protocol.c
TRACKER = ../X32Link/bpm_tracker.c
CFG     = ../X32Link/app_config.c
OSC     = ../X32Link/osc_out.c
MIDICALC = ../X32MidiClock/midi_bpm_calc.c
PUB     = ../X32Link/bpm_publisher.c
OSCIN   = ../X32Link/osc_in.c
FADERDB = ../X32FaderDisp/fader_db.c

.PHONY: all clean

all: test_bpm_tracker test_link_protocol test_app_config test_osc_out test_midi_bpm_calc test_bpm_publisher test_osc_in test_fader_db
	./test_bpm_tracker
	./test_link_protocol
	./test_app_config
	./test_osc_out
	./test_midi_bpm_calc
	./test_bpm_publisher
	./test_osc_in
	./test_fader_db
	./check_docs.sh

test_bpm_tracker: test_bpm_tracker.c $(TRACKER) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_link_protocol: test_link_protocol.c $(PROTO) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_app_config: test_app_config.c $(CFG) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_osc_out: test_osc_out.c $(OSC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_midi_bpm_calc: test_midi_bpm_calc.c $(MIDICALC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_bpm_publisher: test_bpm_publisher.c $(PUB) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_osc_in: test_osc_in.c $(OSCIN) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

test_fader_db: test_fader_db.c $(FADERDB) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@ -lm

clean:
	rm -f test_bpm_tracker test_link_protocol test_app_config test_osc_out test_midi_bpm_calc test_bpm_publisher test_osc_in test_fader_db
