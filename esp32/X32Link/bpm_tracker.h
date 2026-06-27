#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void  bpm_tracker_init(float initial_bpm, float threshold);
bool  bpm_tracker_update(float new_bpm);
float bpm_tracker_get(void);

#ifdef __cplusplus
}
#endif
