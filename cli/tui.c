/* tui.c — ncurses TUI for XAir_ToastSaver */
#include "tui.h"
#include <locale.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

/* Color pair IDs */
#define CP_DIM    1
#define CP_HIGH   2
#define CP_PEAK   3
#define CP_NOTCH  4
#define CP_PASS   5
#define CP_FAIL   6
#define CP_INFO   7
#define CP_NOW    8
#define CP_RUNG   9

/* UTF-8 box / block drawing constants */
#define U_BLOCK  "\xe2\x96\x88"  /* █ */
#define U_DARK   "\xe2\x96\x93"  /* ▓ */
#define U_MED    "\xe2\x96\x92"  /* ▒ */
#define U_LIGHT  "\xe2\x96\x91"  /* ░ */
#define U_DOWN   "\xe2\x96\xbc"  /* ▼ */
#define U_HLINE  "\xe2\x94\x80"  /* ─ */
#define U_LTEE   "\xe2\x94\x9c"  /* ├ */
#define U_RTEE   "\xe2\x94\xa4"  /* ┤ */

static int g_tui_on = 0;

/* ── pure helpers (no ncurses) ──────────────────────────────────────────── */

int tui_bin_to_col(int bin, int cols) {
    if (cols <= 1) return 0;
    return bin * (cols - 1) / 99;
}

int tui_col_to_bin(int col, int cols) {
    if (cols <= 1) return 0;
    return col * 99 / (cols - 1);
}

int tui_excess_level(float excess, float threshold) {
    if (excess < 5.0f)       return 0;
    if (excess >= threshold) return 4;  /* threshold may be < 10 */
    if (excess < 10.0f)      return 1;
    if (excess < 15.0f)      return 2;
    return 3;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

void tui_init(void) {
    if (g_tui_on) return;
    if (!isatty(STDOUT_FILENO)) return;  /* piped/redirected — stay plain text */
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    use_default_colors();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    init_pair(CP_DIM,   -1,           -1);
    init_pair(CP_HIGH,  COLOR_YELLOW, -1);
    init_pair(CP_PEAK,  COLOR_YELLOW, -1);
    init_pair(CP_NOTCH, COLOR_RED,    -1);
    init_pair(CP_PASS,  COLOR_GREEN,  -1);
    init_pair(CP_FAIL,  COLOR_RED,    -1);
    init_pair(CP_INFO,  COLOR_CYAN,   -1);
    init_pair(CP_NOW,   COLOR_CYAN,   -1);
    init_pair(CP_RUNG,  COLOR_YELLOW, -1);

    atexit(tui_shutdown);
    g_tui_on = 1;
}

void tui_shutdown(void) {
    if (!g_tui_on) return;
    curs_set(1);
    endwin();
    g_tui_on = 0;
}

void tui_handle_resize(void) {
    if (!g_tui_on) return;
    endwin();
    refresh();
    clear();
}

/* ── input ──────────────────────────────────────────────────────────────── */

int tui_poll_key(int *running, int *ro_confirmed, int *log_scroll) {
    int ch = getch();
    if (ch == ERR) return ERR;
    if (ch == 'q' || ch == KEY_F(1))              { if (running)      *running = 0; }
    if ((ch == '\n' || ch == KEY_ENTER) && ro_confirmed) { *ro_confirmed = 1; }
    if (ch == KEY_UP   && log_scroll && *log_scroll > 0) (*log_scroll)--;
    if (ch == KEY_DOWN && log_scroll)              (*log_scroll)++;
    return ch;
}

/* ── internal drawing helpers ───────────────────────────────────────────── */

static void draw_hdr(const char *ip, const char *detail, const char *mode) {
    attron(A_REVERSE);
    move(0, 0);
    clrtoeol();
    printw(" XAir Toast Saver  %s%s  %s ", ip, detail, mode);
    int cx, cy; getyx(stdscr, cy, cx); (void)cy;
    for (int c = cx; c < COLS; c++) addch(' ');
    attroff(A_REVERSE);
}

static void draw_sep(int row, const char *label) {
    move(row, 0);
    clrtoeol();
    move(row, 0);
    attron(A_DIM);
    addstr(U_LTEE);
    addstr(U_HLINE);
    if (label && label[0]) {
        addstr(" ");
        addstr(label);
        addstr(" ");
    }
    int cx, cy; getyx(stdscr, cy, cx); (void)cy;
    for (int c = cx; c < COLS - 1; c++) addstr(U_HLINE);
    addstr(U_RTEE);
    attroff(A_DIM);
}

static void draw_spectrum(int row_ruler, int row_spec, int row_mark,
                          const TuiReactiveCtx *ctx) {
    int W = COLS - 2;
    if (W < 4) W = 4;

    /* frequency ruler */
    move(row_ruler, 0);
    clrtoeol();
    addstr(" 20Hz");
    int kHz1_col = 1 + tui_bin_to_col(56, W);
    int cx, cy; getyx(stdscr, cy, cx); (void)cy;
    while (cx < kHz1_col) { addch(' '); cx++; }
    addstr("1kHz");
    getyx(stdscr, cy, cx);
    int end_col = COLS - 6;
    while (cx < end_col && cx < COLS - 1) { addch(' '); cx++; }
    addstr("20kHz");

    /* spectrum row */
    move(row_spec, 0);
    clrtoeol();
    addch(' ');
    if (!ctx->baseline_ready) {
        int bf = ctx->baseline_frames > 0 ? ctx->baseline_frames : 1;
        int prog = ctx->baseline_count * 20 / bf;
        addstr("Calibrating... [");
        for (int i = 0; i < 20; i++) {
            if (i < prog) {
                attron(COLOR_PAIR(CP_PEAK));
                addstr(U_BLOCK);
                attroff(COLOR_PAIR(CP_PEAK));
            } else {
                attron(A_DIM);
                addstr(U_LIGHT);
                attroff(A_DIM);
            }
        }
        printw("]  %d/%d frames", ctx->baseline_count, ctx->baseline_frames);
    } else {
        for (int col = 0; col < W; col++) {
            int bin = tui_col_to_bin(col, W);
            int notched = 0;
            for (int j = 0; j < 31; j++) {
                if (ctx->toast->active[j] && TOAST_GEQ_BIN[j] == bin) { notched = 1; break; }
            }
            if (notched) {
                attron(COLOR_PAIR(CP_NOTCH) | A_BOLD);
                addch('N');
                attroff(COLOR_PAIR(CP_NOTCH) | A_BOLD);
            } else {
                float excess = ctx->bins[bin] - ctx->baseline[bin];
                int lv = tui_excess_level(excess, ctx->threshold_dB);
                switch (lv) {
                case 0: addch(' '); break;
                case 1: attron(A_DIM); addstr(U_LIGHT); attroff(A_DIM); break;
                case 2: addstr(U_MED); break;
                case 3: attron(COLOR_PAIR(CP_HIGH)); addstr(U_DARK); attroff(COLOR_PAIR(CP_HIGH)); break;
                default:
                    attron(COLOR_PAIR(CP_PEAK) | A_BOLD); addstr(U_BLOCK); attroff(COLOR_PAIR(CP_PEAK) | A_BOLD);
                    break;
                }
            }
        }
    }

    /* notch marker row */
    move(row_mark, 0);
    clrtoeol();
    addch(' ');
    if (ctx->baseline_ready) {
        for (int col = 0; col < W; col++) {
            int bin = tui_col_to_bin(col, W);
            int has = 0;
            for (int j = 0; j < 31; j++) {
                if (ctx->toast->active[j] && TOAST_GEQ_BIN[j] == bin) { has = 1; break; }
            }
            if (has) {
                attron(COLOR_PAIR(CP_NOTCH));
                addstr(U_DOWN);
                attroff(COLOR_PAIR(CP_NOTCH));
            } else {
                addch(' ');
            }
        }
    }
}

static void draw_log(int row_sep, int row_end,
                     const char * const *log, int log_n, int log_scroll) {
    draw_sep(row_sep, "Log");
    int avail = row_end - row_sep - 1;
    if (avail <= 0) return;
    int scroll = log_scroll;
    if (scroll < 0) scroll = 0;
    if (scroll > log_n - avail && log_n > avail) scroll = log_n - avail;
    int start_idx = log_n - avail - scroll;
    if (start_idx < 0) start_idx = 0;
    for (int r = 0; r < avail; r++) {
        move(row_sep + 1 + r, 0);
        clrtoeol();
        int idx = start_idx + r;
        if (idx >= 0 && idx < log_n) {
            attron(A_DIM);
            addstr("  ");
            addstr(log[idx]);
            attroff(A_DIM);
        }
    }
}

static void draw_status(int row_sep, const char *state, const char *hints) {
    draw_sep(row_sep, "Status");
    move(row_sep + 1, 0);
    clrtoeol();
    addstr("  ");
    addstr(state);
    if (hints) {
        int hints_len = (int)strlen(hints);
        int cx, cy; getyx(stdscr, cy, cx); (void)cy;
        int target = COLS - hints_len - 2;
        if (target > cx && target < COLS) {
            move(row_sep + 1, target);
            addstr(hints);
        }
    }
}

/* ── public draw functions ──────────────────────────────────────────────── */

void tui_draw_doctor(const char *ip, const DoctorItem *items, int n) {
    if (!g_tui_on) return;
    clear();

    draw_hdr(ip, "", "doctor");

    int row = 2;
    for (int i = 0; i < n && row < LINES - 3; i++, row++) {
        move(row, 0);
        clrtoeol();
        if (items[i].pass == 1) {
            attron(COLOR_PAIR(CP_PASS) | A_BOLD);
            addstr("  [PASS]  ");
            attroff(COLOR_PAIR(CP_PASS) | A_BOLD);
        } else if (items[i].pass == 0) {
            attron(COLOR_PAIR(CP_FAIL) | A_BOLD);
            addstr("  [FAIL]  ");
            attroff(COLOR_PAIR(CP_FAIL) | A_BOLD);
        } else {
            attron(COLOR_PAIR(CP_INFO));
            addstr("  [INFO]  ");
            attroff(COLOR_PAIR(CP_INFO));
        }
        addstr(items[i].text);
    }

    if (n > 0 && row < LINES - 3) {
        int fails = 0;
        for (int i = 0; i < n; i++) if (items[i].pass == 0) fails++;
        row++;
        move(row, 0);
        clrtoeol();
        if (fails == 0) {
            attron(COLOR_PAIR(CP_PASS));
            addstr("  All checks passed.");
            attroff(COLOR_PAIR(CP_PASS));
        } else {
            attron(COLOR_PAIR(CP_FAIL));
            printw("  %d issue%s found.", fails, fails == 1 ? "" : "s");
            attroff(COLOR_PAIR(CP_FAIL));
        }
    }

    draw_status(LINES - 2, "doctor complete", "f fix-all   q quit");
    refresh();
}

void tui_draw_reactive(const TuiReactiveCtx *ctx,
                       const char * const *log, int log_n, int log_scroll) {
    if (!g_tui_on) return;

    char detail[64];
    snprintf(detail, sizeof(detail), "  ch:%d  fx:%d  thr:%.0fdB",
             ctx->channel, ctx->fx_slot, ctx->threshold_dB);
    draw_hdr(ctx->ip, detail, "reactive");

    draw_spectrum(1, 2, 3, ctx);

    /* active notches panel */
    int n_active = 0;
    for (int j = 0; j < 31; j++) if (ctx->toast->active[j]) n_active++;
    int notch_rows = n_active > 4 ? 4 : (n_active > 0 ? n_active : 1);

    draw_sep(4, "Active Notches");
    int row = 5;
    if (n_active == 0) {
        move(row, 0); clrtoeol();
        attron(A_DIM); addstr("  (no active notches)"); attroff(A_DIM);
        row++;
    } else {
        int drawn = 0;
        for (int j = 0; j < 31 && drawn < notch_rows; j++) {
            if (!ctx->toast->active[j]) continue;
            move(row, 0); clrtoeol();
            if (ctx->toast->from_profile[j]) {
                printw("  %-8s  %.0fdB  pinned [profile]",
                       ctx->geq_labels[j], ctx->toast->cut_dB);
            } else if (ctx->toast->release_hold[j] > 0) {
                int f = ctx->toast->release_hold[j];
                int total = ctx->toast->release_frames > 0 ? ctx->toast->release_frames : 1;
                int fill  = f * 20 / total;
                printw("  %-8s  %.0fdB  %.1fs  [",
                       ctx->geq_labels[j], ctx->toast->cut_dB, f / 20.0f);
                for (int p = 0; p < 20; p++) {
                    if (p < fill) {
                        attron(COLOR_PAIR(CP_PEAK));
                        addstr(U_BLOCK);
                        attroff(COLOR_PAIR(CP_PEAK));
                    } else {
                        attron(A_DIM); addstr(U_LIGHT); attroff(A_DIM);
                    }
                }
                addstr("]  releasing");
            } else {
                float age = (float)(ctx->now - ctx->notched_at[j]);
                attron(COLOR_PAIR(CP_NOTCH));
                printw("  %-8s  %.0fdB  %.1fs",
                       ctx->geq_labels[j], ctx->toast->cut_dB, age);
                attroff(COLOR_PAIR(CP_NOTCH));
            }
            row++; drawn++;
        }
    }

    draw_log(4 + 1 + notch_rows, LINES - 2, log, log_n, log_scroll);

    /* status line */
    int n_notch = 0;
    float noise = 0.0f;
    for (int j = 0; j < 31; j++) if (ctx->toast->active[j]) n_notch++;
    if (ctx->baseline_ready) {
        for (int i = 0; i < 100; i++) noise += ctx->baseline[i];
        noise /= 100.0f;
    }
    char state[160];
    if (!ctx->baseline_ready) {
        snprintf(state, sizeof(state), "calibrating  %d/%d frames",
                 ctx->baseline_count, ctx->baseline_frames);
    } else {
        snprintf(state, sizeof(state), "listening · %d notch%s · noise floor %.1f dBFS",
                 n_notch, n_notch == 1 ? "" : "es", noise);
    }
    draw_status(LINES - 2, state, "q quit");
    refresh();
}

void tui_draw_ringout(const TuiRingoutCtx *ctx,
                      const char * const *log, int log_n, int log_scroll) {
    if (!g_tui_on) return;

    char detail[64];
    snprintf(detail, sizeof(detail), "  ch:%d  bus:%d  ceil:%.0fdB",
             ctx->base.channel, ctx->ro_bus, ctx->ceiling_db);
    draw_hdr(ctx->base.ip, detail, "ring-out");

    draw_spectrum(1, 2, 3, &ctx->base);

    /* gain ladder */
    draw_sep(4, "Gain Ladder");

    float range = ctx->ceiling_db - ctx->start_db;
    if (range < 0.5f) range = 0.5f;

    /* How many ladder rows fit between row 5 and LINES-4 (log sep above status) */
    int ladder_max = LINES - 4 - 5 - 1;  /* 4=status rows, 5=ladder start row, 1=log sep */
    if (ladder_max < 2) ladder_max = 2;
    if (ladder_max > 14) ladder_max = 14;

    float step = range / (float)(ladder_max - 1);
    if (step < 1.0f) step = 1.0f;
    int n_rungs = (int)(range / step) + 1;
    if (n_rungs > ladder_max) n_rungs = ladder_max;

    const RingoutState *rst = ctx->rst;
    float cur_gain = rst ? rst->cur_gain_db : ctx->start_db;

    for (int r = 0; r < n_rungs; r++) {
        float gain = ctx->ceiling_db - r * step;
        int rrow = 5 + r;
        if (rrow >= LINES - 4) break;
        move(rrow, 0);
        clrtoeol();

        int is_now     = fabsf(cur_gain - gain) < step * 0.5f;
        int is_ceiling = fabsf(gain - ctx->ceiling_db) < 0.1f;
        int is_start   = fabsf(gain - ctx->start_db)   < 0.5f;

        /* find a notch placed at this gain level */
        int notch_j = -1;
        if (ctx->notch_at_gain && ctx->base.toast) {
            for (int j = 0; j < 31; j++) {
                if (ctx->base.toast->active[j] &&
                    ctx->notch_at_gain[j] != 0.0f &&
                    fabsf(ctx->notch_at_gain[j] - gain) < step * 0.5f) {
                    notch_j = j; break;
                }
            }
        }

        if (is_now) {
            attron(COLOR_PAIR(CP_NOW) | A_BOLD);
            printw("  %+5.1fdB \xe2\x94\xa4 \xe2\x97\x84\xe2\x80\x94 now  %s",
                   gain, ctx->phase_str ? ctx->phase_str : "");
            attroff(COLOR_PAIR(CP_NOW) | A_BOLD);
        } else if (notch_j >= 0) {
            attron(COLOR_PAIR(CP_RUNG));
            printw("  %+5.1fdB \xe2\x94\xa4\xe2\x80\x94\xe2\x80\x94\xe2\x97\x8f %s",
                   gain, ctx->base.geq_labels[notch_j]);
            attroff(COLOR_PAIR(CP_RUNG));
        } else {
            attron(A_DIM);
            printw("  %+5.1fdB \xe2\x94\xa4", gain);
            if (is_ceiling) addstr(" ceiling");
            else if (is_start) addstr(" start");
            attroff(A_DIM);
        }
    }

    int log_sep = 5 + n_rungs;
    draw_log(log_sep, LINES - 2, log, log_n, log_scroll);

    /* status */
    char state[160];
    if (ctx->waiting) {
        snprintf(state, sizeof(state),
                 "WAITING \xe2\x80\x94 press Enter to raise to %.1fdB", ctx->next_gain_db);
        draw_status(LINES - 2, state, "Enter confirm   q abort");
    } else if (rst) {
        float margin = rst->margin_db;
        snprintf(state, sizeof(state), "%s · %d notch%s · margin %.1f/%.1fdB",
                 ctx->phase_str ? ctx->phase_str : "RUNNING",
                 rst->n_notches, rst->n_notches == 1 ? "" : "es",
                 margin, ctx->ceiling_db - ctx->start_db);
        draw_status(LINES - 2, state, "q abort");
    } else {
        draw_status(LINES - 2, "calibrating ringout baseline...", "q abort");
    }
    refresh();
}
