#include "bar.h"

int bar_beats(int quantum, int bars) {
    if (quantum < 1 || bars < 1) return 0;
    return quantum * bars;
}

double bar_ms(float bpm, int quantum, int bars) {
    if (bpm <= 0.0f || quantum < 1 || bars < 1) return 0.0;
    return (double)bars * (double)quantum * 60000.0 / (double)bpm;
}
