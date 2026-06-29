#ifndef LOG_H
#define LOG_H

#define LOG_CAP  200
#define LOG_LINE 128

void               log_push(const char *fmt, ...);
void               log_scroll_down(void);
void               log_scroll_reset(void);
int                log_count(void);
int                log_scroll(void);
int               *log_scroll_ptr(void);
const char *const *log_lines(void);

#endif
