CC     = gcc
RM     = rm -rf
MKDIR  = mkdir -p
BUILD  = build

DIRS   = X32lib
CFLAGS = -O0 -g3 -Wall $(addprefix -I,$(DIRS))
LDFLAGS = $(addprefix -L,$(DIRS)) -lX32 -lm

all: dirs X32lib XAir_ToastSaver XAir XR18

dirs:
	@$(MKDIR) $(BUILD)

X32lib:
	@$(MAKE) -C $@ all

XAir_ToastSaver: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c toast_logic.c ringout_logic.c ringout_profile.c mic_cal.c rta_bins.c osc_io.c tui.c $(LDFLAGS) -lncursesw

XAir: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c $(LDFLAGS)

XR18: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c $(LDFLAGS)

X32Tap: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c x32tap_logic.c $(LDFLAGS) -lncursesw

test_osc_io: X32lib | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c osc_io.c $(LDFLAGS)

test_toast_logic: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c toast_logic.c -lm

test_x32tap: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c x32tap_logic.c -lm

test_ringout_logic: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c ringout_logic.c toast_logic.c -lm

test_rta_bins: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c rta_bins.c toast_logic.c -lm

test_mic_cal: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c mic_cal.c -lm

test_ringout_profile: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c ringout_profile.c -lm

test_tui_helpers: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c -lm

check: test_osc_io test_toast_logic test_x32tap test_ringout_logic test_rta_bins test_mic_cal test_ringout_profile test_tui_helpers
	$(BUILD)/test_osc_io
	$(BUILD)/test_toast_logic
	$(BUILD)/test_x32tap
	$(BUILD)/test_ringout_logic
	$(BUILD)/test_rta_bins
	$(BUILD)/test_mic_cal
	$(BUILD)/test_ringout_profile
	$(BUILD)/test_tui_helpers

clean:
	$(RM) $(BUILD)
	$(MAKE) -C X32lib clean

.PHONY: $(DIRS) check clean dirs

# Ring-out safety harness (T-025): mock X32 + synthetic feedback (integration)
test-ringout-emu: XAir_ToastSaver
	python3 test_ringout_emu.py $(BUILD)/XAir_ToastSaver
