#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

float bpm_to_normalized(float bpm);
int   osc_build_fx_delay(uint8_t* buf, int slot, float norm_val);

#ifdef __cplusplus
}
#endif
