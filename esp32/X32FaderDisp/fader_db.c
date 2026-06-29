#include "fader_db.h"
#include <stdio.h>
#include <math.h>

float fader_to_db(float f) {
    if (f >= 1.0f)    return 10.0f;
    if (f >= 0.5f)    return 40.0f  * f - 30.0f;
    if (f >= 0.25f)   return 80.0f  * f - 50.0f;
    if (f >= 0.0625f) return 160.0f * f - 70.0f;
    if (f >  0.0f)    return 480.0f * f - 90.0f;
    return FADER_DB_MINUS_INF;   /* f <= 0 → -oo */
}

void fader_db_str(float f, char *out, int n) {
    float d = fader_to_db(f);
    if (d <= FADER_DB_MINUS_INF + 0.05f) { snprintf(out, n, "-oo"); return; }
    if (fabsf(d) < 0.05f)                { snprintf(out, n, "0.0"); return; }
    snprintf(out, n, "%+.1f", d);        /* "+10.0", "-6.0" */
}
