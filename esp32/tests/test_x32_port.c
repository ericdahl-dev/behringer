#include <stdio.h>
#include <string.h>
#include <assert.h>

// Stubs for X32.c globals referenced by x32_port.c
char Xip_str[32];
char Xport_str[8];
int  Xdebug;
int  Xverbose;

#include "../X32_emulator/x32_port.h"
#include "../X32_emulator/x32_port.c"

static int passed = 0, failed = 0;

#define PASS(name) do { printf("PASS  %s\n", name); passed++; } while(0)
#define FAIL(name, msg) do { printf("FAIL  %s: %s\n", name, msg); failed++; } while(0)

// --- Behavior 1: IP injection ---

void test_get_ip_writes_xip_str() {
    Xip_str[0] = 0;
    x32_get_ip("192.168.4.1");
    if (strcmp(Xip_str, "192.168.4.1") == 0)
        PASS("get_ip: writes wifi ip to Xip_str");
    else
        FAIL("get_ip: writes wifi ip to Xip_str", Xip_str);
}

void test_get_ip_truncates_to_31_chars() {
    x32_get_ip("123456789012345678901234567890X_overflow");
    if (strlen(Xip_str) <= 31)
        PASS("get_ip: truncates at 31 chars");
    else
        FAIL("get_ip: truncates at 31 chars", "overflowed");
}

void test_get_ip_null_safe() {
    strcpy(Xip_str, "old");
    x32_get_ip(NULL);
    if (strcmp(Xip_str, "old") == 0)
        PASS("get_ip: NULL input leaves Xip_str unchanged");
    else
        FAIL("get_ip: NULL input leaves Xip_str unchanged", Xip_str);
}

// --- Behavior 2: Defaults ---

void test_defaults_sets_port() {
    Xport_str[0] = 0;
    x32_apply_defaults();
    if (strcmp(Xport_str, "10023") == 0)
        PASS("defaults: Xport_str = 10023");
    else
        FAIL("defaults: Xport_str = 10023", Xport_str);
}

void test_defaults_verbose_on() {
    Xverbose = 0;
    x32_apply_defaults();
    if (Xverbose == 1)
        PASS("defaults: Xverbose = 1");
    else
        FAIL("defaults: Xverbose = 1", "wrong value");
}

void test_defaults_debug_off() {
    Xdebug = 99;
    x32_apply_defaults();
    if (Xdebug == 0)
        PASS("defaults: Xdebug = 0");
    else
        FAIL("defaults: Xdebug = 0", "wrong value");
}

// --- Behavior 3: State file path ---

void test_state_file_path_native() {
    if (strcmp(X32_STATE_FILE, ".X32res.rc") == 0)
        PASS("state file: native path = .X32res.rc");
    else
        FAIL("state file: native path", X32_STATE_FILE);
}

int main(void) {
    printf("=== x32_port tests ===\n");
    test_get_ip_writes_xip_str();
    test_get_ip_truncates_to_31_chars();
    test_get_ip_null_safe();
    test_defaults_sets_port();
    test_defaults_verbose_on();
    test_defaults_debug_off();
    test_state_file_path_native();
    printf("=== %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
