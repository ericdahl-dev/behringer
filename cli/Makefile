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
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c toast_logic.c ringout_logic.c $(LDFLAGS)

XAir: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c $(LDFLAGS)

XR18: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c $(LDFLAGS)

X32Tap: X32lib
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c x32tap_logic.c $(LDFLAGS) -lncursesw

test_toast_logic: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c toast_logic.c -lm

test_x32tap: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c x32tap_logic.c -lm

test_ringout_logic: | dirs
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $@.c ringout_logic.c toast_logic.c -lm

check: test_toast_logic test_x32tap test_ringout_logic
	$(BUILD)/test_toast_logic
	$(BUILD)/test_x32tap
	$(BUILD)/test_ringout_logic

clean:
	$(RM) $(BUILD)
	$(MAKE) -C X32lib clean

.PHONY: $(DIRS) check clean dirs
