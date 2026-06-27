/*
 * XAir_ToastSaver.c
 *
 * Automatic feedback destroyer for wedding toasts on the XR18.
 * Subscribes to /meters/4 RTA (100 bins, ~20 Hz), detects feedback spikes,
 * and applies notch cuts via a TEQ inserted in a configured FX slot.
 *
 * Usage: XAir_ToastSaver -i <ip> [-c <ch 1-18>] [-s <slot 1-4>]
 *                        [-t <threshold_dB>] [-d <cut_dB>] [-r <release_sec>]
 *                        [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include "toast_logic.h"

extern int Xsprint(char *bd, int index, char format, void *bs);

/* ── constants ──────────────────────────────────────────────────────────── */

#define PORT             10024
#define BSIZE            512
#define XREMOTE_TIMEOUT  9      /* send /xremote every 9 seconds */
#define BASELINE_FRAMES  40     /* ~2 seconds at 20 Hz to establish baseline */
#define CONFIRM_FRAMES    3     /* consecutive frames a peak must persist before notching (~150ms) */
#define NARROW_DB        10.0f  /* dB advantage over neighbors required to classify as feedback */

/* ── GEQ labels (display/logging only) ──────────────────────────────────── */

static const char *GEQ_LABEL[31] = {
    "20Hz",  "25Hz",  "31.5Hz","40Hz",  "50Hz",  "63Hz",  "80Hz",
    "100Hz", "125Hz", "160Hz", "200Hz", "250Hz", "315Hz", "400Hz",
    "500Hz", "630Hz", "800Hz", "1kHz",  "1.25kHz","1.6kHz","2kHz",
    "2.5kHz","3.15kHz","4kHz", "5kHz",  "6.3kHz","8kHz",  "10kHz",
    "12.5kHz","16kHz","20kHz"
};

/* ── types ──────────────────────────────────────────────────────────────── */

typedef struct {
    int    active;
    float  cut_val;       /* float sent to TEQ (0.5 = flat) */
    double notched_at;    /* gettimeofday seconds */
    int    release_hold;  /* consecutive frames below release threshold */
} NotchState;

typedef struct {
    /* config */
    char   ip[20];
    int    channel;
    int    fx_slot;
    float  threshold_dB;
    float  cut_dB;
    float  release_sec;
    int    verbose;

    /* socket */
    int                fd;
    struct sockaddr_in xip;
    struct sockaddr   *xip_addr;
    socklen_t          xip_len;

    /* RTA */
    float   bins[100];
    float   baseline[100];
    float   baseline_accum[100];
    int     baseline_count;
    int     baseline_ready;

    /* notch state */
    NotchState notches[31];  /* index 0 = GEQ par 01 */

    /* feedback confirmation counters — consecutive frames this par has been
     * the narrow peak; resets to 0 for all pars when a different par leads */
    int confirm_hold[31];
} AppState;

/* ── globals ─────────────────────────────────────────────────────────────── */

static volatile int g_running = 1;

static void sig_handler(int sig) { (void)sig; g_running = 0; }

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* ── OSC send helpers ────────────────────────────────────────────────────── */

static void osc_send_raw(AppState *s, const char *buf, int len) {
    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
}

static void osc_no_args(AppState *s, const char *path) {
    char buf[BSIZE];
    int len = Xsprint(buf, 0, 's', (void *)path);
    osc_send_raw(s, buf, len);
}

static void osc_int(AppState *s, const char *path, int val) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",i");
    len = Xsprint(buf, len, 'i', &val);
    osc_send_raw(s, buf, len);
}

static void osc_float(AppState *s, const char *path, float val) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",f");
    len = Xsprint(buf, len, 'f', &val);
    osc_send_raw(s, buf, len);
}

static void osc_int_float(AppState *s, const char *path, int ival, float fval) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",if");
    len = Xsprint(buf, len, 'i', &ival);
    len = Xsprint(buf, len, 'f', &fval);
    osc_send_raw(s, buf, len);
}

static void send_teq_band(AppState *s, int par, float val) {
    char path[32];
    snprintf(path, sizeof(path), "/fx/%d/par/%02d", s->fx_slot, par);
    osc_float(s, path, val);
}

static void subscribe_meters4(AppState *s) {
    char buf[BSIZE];
    int zero = 0;
    int len = 0;
    len = Xsprint(buf, len, 's', "/meters");
    len = Xsprint(buf, len, 's', ",siii");
    len = Xsprint(buf, len, 's', "/meters/4");
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    osc_send_raw(s, buf, len);
}

/* ── ANSI display ────────────────────────────────────────────────────────── */

/* Print the static header rows (once on startup). */
static void print_static_display(const AppState *s) {
    printf("\033[2J\033[H\033[?25l");
    printf("XAir Toast Saver  ch:%-2d  fx:%d  thr:%.0fdB  cut:%.0fdB  rel:%.0fs\n",
           s->channel, s->fx_slot, s->threshold_dB, s->cut_dB, s->release_sec);
    /* 100-char separator using box-drawing char U+2500 */
    for (int i = 0; i < 100; i++) fputs("\xe2\x94\x80", stdout);
    printf("\n");
    /* frequency ruler: "20Hz" at col 0, "1kHz" centered at col 54, "20kHz" at col 95 */
    printf("20Hz");
    for (int i = 4; i < 54; i++) putchar(' ');
    printf("1kHz");
    for (int i = 58; i < 95; i++) putchar(' ');
    printf("20kHz\n");
    printf("\n");  /* row 4: blank */
    fflush(stdout);
}

/* Redraw rows 5-7 in-place. Call each meter frame. */
static void draw_display(const AppState *s) {
    /* row 5: RTA bar or calibration progress */
    printf("\033[5;1H\033[2K");
    if (!s->baseline_ready) {
        int prog = s->baseline_count * 20 / BASELINE_FRAMES;
        printf("Calibrating... [");
        for (int i = 0; i < 20; i++)
            fputs(i < prog ? "\xe2\x96\x88" : "\xe2\x96\x91", stdout);
        printf("] %d/%d frames", s->baseline_count, BASELINE_FRAMES);
    } else {
        for (int i = 0; i < 100; i++) {
            /* check if this bin belongs to an active notch */
            int notched = 0;
            for (int j = 0; j < 31; j++) {
                if (s->notches[j].active && TOAST_GEQ_BIN[j] == i) { notched = 1; break; }
            }
            if (notched) {
                fputs("\033[31mN\033[0m", stdout);
            } else {
                float excess = s->bins[i] - s->baseline[i];
                if (excess >= s->threshold_dB)
                    fputs("\033[33m\xe2\x96\x88\033[0m", stdout); /* yellow █ */
                else if (excess >= 15.0f)
                    fputs("\xe2\x96\x93", stdout);                 /* ▓ */
                else if (excess >= 10.0f)
                    fputs("\xe2\x96\x92", stdout);                 /* ▒ */
                else if (excess >= 5.0f)
                    fputs("\xe2\x96\x91", stdout);                 /* ░ */
                else
                    putchar(' ');
            }
        }
    }

    /* row 6: notch marker row (▼ at each active notch's RTA bin) */
    printf("\n\033[2K");
    if (s->baseline_ready) {
        for (int i = 0; i < 100; i++) {
            int has_notch = 0;
            for (int j = 0; j < 31; j++) {
                if (s->notches[j].active && TOAST_GEQ_BIN[j] == i) { has_notch = 1; break; }
            }
            if (has_notch)
                fputs("\033[31m\xe2\x96\xbc\033[0m", stdout);  /* red ▼ */
            else
                putchar(' ');
        }
    }

    /* row 7: status line */
    printf("\n\033[2KNotched: ");
    int any = 0;
    if (s->baseline_ready) {
        double t = now_sec();
        for (int j = 0; j < 31; j++) {
            if (s->notches[j].active) {
                printf("%s(%.0fdB %.1fs)  ", GEQ_LABEL[j], s->cut_dB,
                       t - s->notches[j].notched_at);
                any = 1;
            }
        }
    }
    if (!any) printf("(none)");

    fflush(stdout);
}

/* ── detection and notch control ─────────────────────────────────────────── */

static void run_detection(AppState *s) {
    int release_frames = (int)(s->release_sec * 20.0f);
    float release_thr  = s->threshold_dB / 2.0f;

    /* release pass: check all active notches */
    for (int j = 0; j < 31; j++) {
        if (!s->notches[j].active) continue;
        int bin = TOAST_GEQ_BIN[j];
        if (s->bins[bin] < s->baseline[bin] + release_thr) {
            s->notches[j].release_hold++;
            /* Smooth ramp from cut_val toward flat (0.5) over the release window */
            float t = (float)s->notches[j].release_hold / (float)release_frames;
            if (t > 1.0f) t = 1.0f;
            float ramp_val = s->notches[j].cut_val +
                             (0.5f - s->notches[j].cut_val) * t;
            send_teq_band(s, j + 1, ramp_val);
            if (s->notches[j].release_hold >= release_frames) {
                double elapsed = now_sec() - s->notches[j].notched_at;
                if (!s->verbose)
                    printf("\033[8;1H\033[2K");
                printf("[release] par=%02d (%s) after %.1fs\n",
                       j + 1, GEQ_LABEL[j], elapsed);
                if (!s->verbose) fflush(stdout);
                s->notches[j].active = 0;
                s->notches[j].release_hold = 0;
            }
        } else {
            if (s->notches[j].release_hold > 0) {
                /* Feedback returned mid-fade — snap back to full cut */
                s->notches[j].release_hold = 0;
                send_teq_band(s, j + 1, s->notches[j].cut_val);
            }
        }
    }

    /* detection pass: find peak bin above threshold */
    int peak_bin = -1;
    if (!detect_peak(s->bins, s->baseline, s->threshold_dB, &peak_bin)) {
        /* no spike this frame — clear all confirmation counters */
        for (int j = 0; j < 31; j++) s->confirm_hold[j] = 0;
        return;
    }

    /* narrowness gate — reject broadband transients (speech, claps, music) */
    if (!is_narrow_peak(s->bins, 100, peak_bin, 2, 3, NARROW_DB)) {
        for (int j = 0; j < 31; j++) s->confirm_hold[j] = 0;
        return;
    }

    /* accumulate confirmation for this par; reset all others */
    int par = bin_to_geq_par(peak_bin);
    for (int j = 0; j < 31; j++)
        s->confirm_hold[j] = (j == par - 1) ? s->confirm_hold[j] + 1 : 0;

    int j = par - 1;
    if (s->notches[j].active) return;           /* already notched */
    if (s->confirm_hold[j] < CONFIRM_FRAMES) return;  /* not confirmed yet */

    s->confirm_hold[j] = 0;
    float cut_float = db_to_geq_float(s->cut_dB);
    send_teq_band(s, par, cut_float);

    s->notches[j].active       = 1;
    s->notches[j].cut_val      = cut_float;
    s->notches[j].notched_at   = now_sec();
    s->notches[j].release_hold = 0;

    if (!s->verbose)
        printf("\033[8;1H\033[2K");
    printf("[notch] par=%02d (%s) cut=%.0fdB  bin=%d  level=%.1fdBFS\n",
           par, GEQ_LABEL[j], s->cut_dB, peak_bin, s->bins[peak_bin]);
    if (!s->verbose) fflush(stdout);
}

static void cleanup_notches(AppState *s) {
    int n = 0;
    for (int j = 0; j < 31; j++) {
        if (s->notches[j].active) {
            send_teq_band(s, j + 1, 0.5f);
            s->notches[j].active = 0;
            n++;
        }
    }
    if (n > 0)
        fprintf(stderr, "\n[cleanup] restored %d band(s) to flat\n", n);
}

/* ── /meters/4 blob handler ──────────────────────────────────────────────── */

static void handle_meters4(const uint8_t *blob, int blen, AppState *s) {
    if (parse_meters4_blob(blob, blen, s->bins) != 0) return;

    /* baseline calibration */
    if (!s->baseline_ready) {
        for (int i = 0; i < 100; i++)
            s->baseline_accum[i] += s->bins[i];
        s->baseline_count++;
        if (s->baseline_count >= BASELINE_FRAMES) {
            for (int i = 0; i < 100; i++)
                s->baseline[i] = s->baseline_accum[i] / BASELINE_FRAMES;
            s->baseline_ready = 1;
            float sum = 0.0f;
            for (int i = 0; i < 100; i++) sum += s->baseline[i];
            if (!s->verbose)
                printf("\033[8;1H\033[2K");
            printf("[baseline] established: avg noise floor = %.1f dBFS\n",
                   sum / 100.0f);
            if (!s->verbose) fflush(stdout);
        }
        if (!s->verbose) draw_display(s);
        return;
    }

    /* exponential baseline update */
    update_baseline(s->baseline, s->bins, 100, 0.005f);

    if (s->verbose) {
        int peak = 0;
        for (int i = 1; i < 100; i++)
            if (s->bins[i] > s->bins[peak]) peak = i;
        printf("peak bin=%d  level=%.1f dBFS  baseline=%.1f dBFS  excess=%.1f dB\n",
               peak, s->bins[peak], s->baseline[peak],
               s->bins[peak] - s->baseline[peak]);
    }

    run_detection(s);
    if (!s->verbose) draw_display(s);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    AppState s;
    memset(&s, 0, sizeof(s));

    /* defaults */
    strcpy(s.ip, "192.168.0.64");
    s.channel      = 1;
    s.fx_slot      = 4;
    s.threshold_dB = 20.0f;
    s.cut_dB       = -6.0f;
    s.release_sec  = 10.0f;

    int opt, ip_set = 0;
    while ((opt = getopt(argc, argv, "i:c:s:t:d:r:vh")) != -1) {
        switch (opt) {
        case 'i': strncpy(s.ip, optarg, sizeof(s.ip) - 1); ip_set = 1; break;
        case 'c': s.channel      = atoi(optarg); break;
        case 's': s.fx_slot      = atoi(optarg); break;
        case 't': s.threshold_dB = atof(optarg); break;
        case 'd': s.cut_dB       = atof(optarg); break;
        case 'r': s.release_sec  = atof(optarg); break;
        case 'v': s.verbose      = 1;            break;
        default:
        case 'h':
            fprintf(stderr,
                "usage: XAir_ToastSaver -i <ip> [-c <ch 1-18>] [-s <slot 1-4>]\n"
                "                       [-t <threshold_dB>] [-d <cut_dB>]\n"
                "                       [-r <release_sec>] [-v]\n"
                "  -i  XR18 IP address (required)\n"
                "  -c  RTA source channel 1-18 (default: 1)\n"
                "  -s  FX slot with TEQ, 1-4 (default: 4)\n"
                "  -t  spike threshold in dB above noise floor (default: 20)\n"
                "  -d  notch cut depth in dB, negative (default: -6)\n"
                "  -r  seconds before a notch is released (default: 10)\n"
                "  -v  verbose text output (disables live display)\n");
            return opt == 'h' ? 0 : 1;
        }
    }

    if (!ip_set) {
        fprintf(stderr, "error: -i <ip> is required\n");
        return 1;
    }
    if (s.channel < 1 || s.channel > 18) {
        fprintf(stderr, "error: channel must be 1-18\n"); return 1;
    }
    if (s.fx_slot < 1 || s.fx_slot > 4) {
        fprintf(stderr, "error: fx slot must be 1-4\n"); return 1;
    }
    if (s.cut_dB > 0.0f) {
        fprintf(stderr, "warning: cut_dB should be negative; got %.1f\n", s.cut_dB);
    }

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* ── create UDP socket ── */
    if ((s.fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }
    memset(&s.xip, 0, sizeof(s.xip));
    s.xip.sin_family      = AF_INET;
    s.xip.sin_addr.s_addr = inet_addr(s.ip);
    s.xip.sin_port        = htons(PORT);
    s.xip_addr            = (struct sockaddr *)&s.xip;
    s.xip_len             = sizeof(s.xip);

    if (!s.verbose)
        printf("XAir Toast Saver  connecting to %s  ch:%d  slot:%d  "
               "threshold:%.0fdB  cut:%.0fdB  release:%.0fs\n",
               s.ip, s.channel, s.fx_slot, s.threshold_dB, s.cut_dB, s.release_sec);

    /* ── /xinfo handshake ── */
    {
        char buf[BSIZE];
        int  len = Xsprint(buf, 0, 's', "/xinfo");
        struct timeval tv = {1, 0};
        fd_set fds;
        FD_ZERO(&fds); FD_SET(s.fd, &fds);

        sendto(s.fd, buf, len, 0, s.xip_addr, s.xip_len);
        if (select(s.fd + 1, &fds, NULL, NULL, &tv) <= 0 ||
            recvfrom(s.fd, buf, BSIZE, 0, 0, 0) < 0 ||
            strcmp(buf, "/xinfo") != 0) {
            fprintf(stderr, "error: no /xinfo reply from %s\n", s.ip);
            close(s.fd);
            return 1;
        }
        if (s.verbose) printf("connected to %s\n", s.ip);
    }

    /* ── configure RTA source and subscribe ── */
    osc_int(&s, "/-stat/rta/source", s.channel);
    osc_int_float(&s, "/-prefs/rta", 0, 0.25f);  /* PEAK mode, decay 0.25 */
    subscribe_meters4(&s);
    osc_no_args(&s, "/xremote");

    if (!s.verbose) print_static_display(&s);

    /* ── main loop ── */
    double last_xremote = now_sec();
    char r_buf[BSIZE];

    while (g_running) {
        /* keepalive */
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_no_args(&s, "/xremote");
            subscribe_meters4(&s);  /* re-subscribe alongside keepalive */
            last_xremote = t;
        }

        /* poll */
        struct timeval tv = {0, 60000};  /* 60 ms */
        fd_set fds;
        FD_ZERO(&fds); FD_SET(s.fd, &fds);
        if (select(s.fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;

        int r_len = recvfrom(s.fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0) continue;
        if (strcmp(r_buf, "/meters/4") != 0) continue;

        /* locate blob: skip OSC address (4-byte padded) and type tag (4-byte padded) */
        int addr_padded = ((int)strlen(r_buf) + 1 + 3) & ~3;
        int tag_padded  = ((int)strlen(r_buf + addr_padded) + 1 + 3) & ~3;
        int blob_start  = addr_padded + tag_padded;
        if (blob_start >= r_len) continue;

        handle_meters4((uint8_t *)r_buf + blob_start, r_len - blob_start, &s);
    }

    /* ── shutdown ── */
    cleanup_notches(&s);
    close(s.fd);
    if (!s.verbose) {
        printf("\033[9;1H\033[?25h");  /* show cursor below display */
        fflush(stdout);
    }
    printf("toast saver stopped\n");
    return 0;
}
