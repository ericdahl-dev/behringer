#include "rta_bins.h"
#include <math.h>

/* Log-frequency interpolation of the 31 ISO 1/3-octave GEQ anchors
 * (TOAST_GEQ_BIN ↔ ISO centers), 1 kHz pinned to bin 56. Off-anchor bins are
 * modelled, not measured — verify with a console sweep via --rta-probe. */
const float RTA_BIN_FREQ[RTA_BIN_COUNT] = {
        20.0f,     21.5f,     23.2f,     25.0f,     26.5f,     28.1f,     29.7f,     31.5f,
        34.1f,     36.9f,     40.0f,     43.1f,     46.4f,     50.0f,     54.0f,     58.3f,
        63.0f,     66.9f,     71.0f,     75.4f,     80.0f,     86.2f,     92.8f,    100.0f,
       107.7f,    116.0f,    125.0f,    133.0f,    141.4f,    150.4f,    160.0f,    172.4f,
       185.7f,    200.0f,    215.4f,    232.1f,    250.0f,    270.0f,    291.6f,    315.0f,
       334.4f,    355.0f,    376.8f,    400.0f,    430.9f,    464.2f,    500.0f,    540.0f,
       583.3f,    630.0f,    668.8f,    709.9f,    753.6f,    800.0f,    861.8f,    928.3f,
      1000.0f,   1077.2f,   1160.4f,   1250.0f,   1329.6f,   1414.2f,   1504.2f,   1600.0f,
      1723.5f,   1856.6f,   2000.0f,   2154.4f,   2320.8f,   2500.0f,   2700.2f,   2916.4f,
      3150.0f,   3343.9f,   3549.6f,   3768.1f,   4000.0f,   4308.9f,   4641.6f,   5000.0f,
      5400.4f,   5832.9f,   6300.0f,   6687.7f,   7099.3f,   7536.2f,   8000.0f,   8617.7f,
      9283.2f,  10000.0f,  10772.2f,  11604.0f,  12500.0f,  13295.7f,  14142.1f,  15042.4f,
     16000.0f,  17235.5f,  18566.4f,  20000.0f,
};

int rta_bin_for_freq(float hz) {
    if (hz <= RTA_BIN_FREQ[0]) return 0;
    if (hz >= RTA_BIN_FREQ[RTA_BIN_COUNT - 1]) return RTA_BIN_COUNT - 1;
    int best = 0;
    float best_d = 1e30f;
    float lh = log10f(hz);
    for (int i = 0; i < RTA_BIN_COUNT; i++) {
        float d = fabsf(log10f(RTA_BIN_FREQ[i]) - lh);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}
