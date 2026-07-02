// Pure builder for the /status JSON payload (LNK-022) — see web_config.cpp.
// Host-testable: no Arduino/WebServer dependency, just snprintf. Same
// "pull the formatting logic out of the .ino-adjacent file" pattern as
// led_phase.h/.c (LNK-021).
#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Formats {"bpm":F,"phase":F,"valid":bool,"quantum":N} into buf. `phase` and
// `quantum` are passed through as-is (caller decides what to do with
// tempo_source_phase()'s -1.0f "no reading yet" sentinel — this function
// doesn't special-case it, it just prints whatever float it's given;
// `valid` is the field JS actually gates on). Returns the snprintf() return
// value (bytes that would have been written, excluding the terminator), so
// the caller can detect truncation the same way any snprintf caller would.
int web_status_json(char* buf, size_t buf_len, float bpm, float phase, bool valid, int quantum);

#ifdef __cplusplus
}
#endif
