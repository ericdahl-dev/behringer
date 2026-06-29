#include <stdarg.h>
#include <stdio.h>
#include "log.h"

static char        g_log[LOG_CAP][LOG_LINE];
static const char *g_log_ptrs[LOG_CAP];
static int         g_log_head   = 0;
static int         g_log_count  = 0;
static int         g_log_scroll = 0;

void log_push(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_log[g_log_head], LOG_LINE, fmt, ap);
    va_end(ap);
    g_log[g_log_head][LOG_LINE - 1] = '\0';
    g_log_head = (g_log_head + 1) % LOG_CAP;
    if (g_log_count < LOG_CAP) g_log_count++;
    if (g_log_scroll > 0) g_log_scroll++;
    for (int i = 0; i < g_log_count; i++)
        g_log_ptrs[i] = g_log[(g_log_head - g_log_count + i + LOG_CAP) % LOG_CAP];
}

void log_scroll_down(void)         { g_log_scroll++; }
void log_scroll_reset(void)        { g_log_scroll = 0; }
int  log_count(void)               { return g_log_count; }
int  log_scroll(void)              { return g_log_scroll; }
int *log_scroll_ptr(void)          { return &g_log_scroll; }
const char *const *log_lines(void) { return (const char *const *)g_log_ptrs; }
