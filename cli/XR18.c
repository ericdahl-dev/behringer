/*
 * XR18.c  --  Behringer XR18 emulator for XAir_Command interactive use
 *
 * Listens on UDP port 10024 (XAir standard port).
 *
 * Handles:
 *   /xinfo           -- identity handshake (required by XAir_Command connect loop)
 *   /status          -- alias for /xinfo
 *   /info            -- simple echo
 *   /xremote         -- register client for meter data pushes
 *   /meters          -- subscribe to meter channel
 *   /fx/N/type       -- FX slot type (default: 10 = DLY)
 *   /fx/N/par/NN     -- FX parameter set/get
 *   <any other path> -- generic get/set via flat param store
 *
 * XR18 topology (for reference):
 *   /ch/01-16        16 mono input channels
 *   /rtn/aux         stereo USB return (ch 17-18)
 *   /rtn/1-4         4 FX returns
 *   /bus/1-6         6 mix buses (single digit, no leading zero)
 *   /fxsend/1-4      4 FX send buses
 *   /lr/             main L/R
 *   /dca/1-4         4 DCAs
 *   /fx/1-4          4 FX processor slots
 *   /headamp/01-18   18 head amp channels
 *
 * Usage: XR18 [-i ip] [-v 0|1] [-l level] [-f fx_type]
 *   -l  simulated meter level 0.0-1.0 (use >0 to trigger XTap auto-tap)
 *   -f  FX type reported for all slots (default 10 = DLY)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

extern int Xsprint(char *bd, int index, char format, void *bs);
extern int Xfprint(char *bd, int index, char *text, char format, void *bs);

/* ── constants ──────────────────────────────────────────────────────────── */

#define PORT        10024
#define BSIZE       512
#define MAX_CLIENTS 4
#define MAX_PARAMS  2048
#define XREMOTE_TTL 10          /* seconds a /xremote subscription stays valid */

#define XVERSION    "1.17"
#define XMODEL      "XR18"
#define XNAME       "XR18 Emulator"

/* ── param store ────────────────────────────────────────────────────────── */

typedef struct {
    char path[64];
    char type;          /* 'i', 'f', 's'; 0 = slot unused */
    union {
        int   i;
        float f;
        char  s[24];
    } v;
} Param;

static Param params[MAX_PARAMS];
static int   n_params = 0;

static Param *param_find(const char *path)
{
    int i;
    for (i = 0; i < n_params; i++)
        if (params[i].type && strcmp(params[i].path, path) == 0)
            return &params[i];
    return NULL;
}

static Param *param_get_or_create(const char *path)
{
    Param *p = param_find(path);
    if (p) return p;
    if (n_params >= MAX_PARAMS) return NULL;
    p = &params[n_params++];
    memset(p, 0, sizeof(*p));
    strncpy(p->path, path, sizeof(p->path) - 1);
    return p;
}

static void param_set_i(const char *path, int v)
{
    Param *p = param_get_or_create(path);
    if (!p) return;
    p->type = 'i';
    p->v.i  = v;
}

static void param_set_f(const char *path, float v)
{
    Param *p = param_get_or_create(path);
    if (!p) return;
    p->type = 'f';
    p->v.f  = v;
}

static void param_set_s(const char *path, const char *v)
{
    Param *p = param_get_or_create(path);
    if (!p) return;
    p->type = 's';
    strncpy(p->v.s, v, sizeof(p->v.s) - 1);
}

/* ── defaults ───────────────────────────────────────────────────────────── */

static void init_defaults(void)
{
    char path[64];
    int  ch, bus, fx, rtn;

    /* console */
    param_set_s("/config/name", "XR18 Emulator");

    /* 16 mono input channels */
    for (ch = 1; ch <= 16; ch++) {
        char name[16];
        sprintf(name, "Ch %02d", ch);
        sprintf(path, "/ch/%02d/config/name", ch);   param_set_s(path, name);
        sprintf(path, "/ch/%02d/config/icon", ch);   param_set_i(path, 0);
        sprintf(path, "/ch/%02d/config/color", ch);  param_set_i(path, 0);
        sprintf(path, "/ch/%02d/config/source", ch); param_set_i(path, ch);

        sprintf(path, "/ch/%02d/preamp/trim", ch);   param_set_f(path, 0.f);
        sprintf(path, "/ch/%02d/preamp/invert", ch); param_set_i(path, 0);
        sprintf(path, "/ch/%02d/preamp/hpon", ch);   param_set_i(path, 0);
        sprintf(path, "/ch/%02d/preamp/hpf", ch);    param_set_f(path, 80.f);

        sprintf(path, "/ch/%02d/gate/on", ch);       param_set_i(path, 0);
        sprintf(path, "/ch/%02d/dyn/on", ch);        param_set_i(path, 0);
        sprintf(path, "/ch/%02d/eq/on", ch);         param_set_i(path, 1);

        /* main mix */
        sprintf(path, "/ch/%02d/mix/on", ch);        param_set_i(path, 1);
        sprintf(path, "/ch/%02d/mix/fader", ch);     param_set_f(path, 0.75f);
        sprintf(path, "/ch/%02d/mix/pan", ch);       param_set_f(path, 0.5f);

        /* bus sends 1-6 */
        for (bus = 1; bus <= 6; bus++) {
            sprintf(path, "/ch/%02d/mix/%02d/on", ch, bus);    param_set_i(path, 1);
            sprintf(path, "/ch/%02d/mix/%02d/level", ch, bus); param_set_f(path, 0.75f);
            sprintf(path, "/ch/%02d/mix/%02d/pan", ch, bus);   param_set_f(path, 0.5f);
        }
        /* FX sends 07-10 */
        for (fx = 7; fx <= 10; fx++) {
            sprintf(path, "/ch/%02d/mix/%02d/level", ch, fx);  param_set_f(path, 0.f);
        }
    }

    /* stereo USB return */
    param_set_s("/rtn/aux/config/name", "USB Rtn");
    param_set_i("/rtn/aux/mix/on",      1);
    param_set_f("/rtn/aux/mix/fader",   0.75f);
    param_set_f("/rtn/aux/mix/pan",     0.5f);

    /* 4 FX returns */
    for (rtn = 1; rtn <= 4; rtn++) {
        char name[16];
        sprintf(name, "FX Rtn %d", rtn);
        sprintf(path, "/rtn/%d/config/name", rtn); param_set_s(path, name);
        sprintf(path, "/rtn/%d/mix/on",      rtn); param_set_i(path, 1);
        sprintf(path, "/rtn/%d/mix/fader",   rtn); param_set_f(path, 0.75f);
        sprintf(path, "/rtn/%d/mix/pan",     rtn); param_set_f(path, 0.5f);
    }

    /* 6 mix buses — single digit, no leading zero */
    for (bus = 1; bus <= 6; bus++) {
        char name[16];
        sprintf(name, "Bus %d", bus);
        sprintf(path, "/bus/%d/config/name", bus); param_set_s(path, name);
        sprintf(path, "/bus/%d/mix/on",      bus); param_set_i(path, 1);
        sprintf(path, "/bus/%d/mix/fader",   bus); param_set_f(path, 0.75f);
        sprintf(path, "/bus/%d/mix/pan",     bus); param_set_f(path, 0.5f);
        sprintf(path, "/bus/%d/eq/on",       bus); param_set_i(path, 1);
    }

    /* 4 FX sends */
    for (fx = 1; fx <= 4; fx++) {
        char name[16];
        sprintf(name, "FX Snd %d", fx);
        sprintf(path, "/fxsend/%d/config/name", fx); param_set_s(path, name);
        sprintf(path, "/fxsend/%d/mix/on",      fx); param_set_i(path, 1);
        sprintf(path, "/fxsend/%d/mix/fader",   fx); param_set_f(path, 0.75f);
    }

    /* main LR */
    param_set_s("/lr/config/name", "Main L/R");
    param_set_i("/lr/mix/on",      1);
    param_set_f("/lr/mix/fader",   0.75f);
    param_set_f("/lr/mix/pan",     0.5f);

    /* 4 DCAs */
    for (ch = 1; ch <= 4; ch++) {
        char name[16];
        sprintf(name, "DCA %d", ch);
        sprintf(path, "/dca/%d/config/name", ch); param_set_s(path, name);
        sprintf(path, "/dca/%d/on",          ch); param_set_i(path, 1);
        sprintf(path, "/dca/%d/fader",       ch); param_set_f(path, 0.75f);
    }

    /* 4 FX slots */
    for (fx = 1; fx <= 4; fx++) {
        sprintf(path, "/fx/%d/type", fx); param_set_i(path, 10); /* 10 = DLY */
        sprintf(path, "/fx/%d/par/02", fx); param_set_f(path, 0.f);
    }
}

/* ── network state ──────────────────────────────────────────────────────── */

static int     Xfd;
static int     Xverbose = 1;
static char    Xip_str[32] = "";
static float   sim_level   = 0.0f;
static int     fx_type_default = 10;
static int     keep_on     = 1;

static struct sockaddr_in server_ip, client_ip;
static socklen_t          client_ip_len = sizeof(client_ip);
static char r_buf[BSIZE], s_buf[BSIZE];
static int  r_len, s_len;

/* xremote subscriber list */
typedef struct {
    struct sockaddr addr;
    time_t          expire;
} XClient;

static XClient clients[MAX_CLIENTS];

static void client_refresh(void)
{
    time_t now = time(NULL);
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].expire &&
            memcmp(&clients[i].addr, &client_ip, sizeof(struct sockaddr)) == 0) {
            clients[i].expire = now + XREMOTE_TTL;
            return;
        }
    }
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].expire || now >= clients[i].expire) {
            clients[i].addr   = *(struct sockaddr *)&client_ip;
            clients[i].expire = now + XREMOTE_TTL;
            if (Xverbose)
                printf("   /xremote registered slot %d  (%s)\n",
                       i, inet_ntoa(client_ip.sin_addr));
            return;
        }
    }
}

static void xsend_current(void)
{
    sendto(Xfd, s_buf, s_len, 0,
           (struct sockaddr *)&client_ip, client_ip_len);
}

static void xbroadcast(void)
{
    time_t now = time(NULL);
    int i;
    xsend_current();
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].expire && now < clients[i].expire &&
            memcmp(&clients[i].addr, &client_ip, sizeof(struct sockaddr)) != 0)
            sendto(Xfd, s_buf, s_len, 0, &clients[i].addr, sizeof(struct sockaddr));
    }
}

/* ── meter push ─────────────────────────────────────────────────────────── */

/* /meters/6 blob: 20-byte header + 40-byte blob.
 * Gate meter float lives at packet offset 28 (blob data offset 8). */
static int build_meter_packet(char *buf, float level)
{
    memset(buf, 0, 64);
    memcpy(buf, "/meters/6", 9);
    buf[12] = ','; buf[13] = 'b';
    buf[16] = 0; buf[17] = 0; buf[18] = 0; buf[19] = 40;
    memcpy(buf + 28, &level, 4);
    return 60;
}

/* ── OSC incoming helpers ────────────────────────────────────────────────── */

/* Position of the type-tag string in r_buf (the ',f', ',i', etc.) */
static int osc_tag_pos(void)
{
    return (strlen(r_buf) + 4) & ~3;
}

/* Returns non-zero if r_buf contains a SET (has a type tag with data). */
static int osc_is_set(void)
{
    int p = osc_tag_pos();
    return (p + 1 < r_len) && (r_buf[p] == ',') && (r_buf[p + 1] != 0);
}

/* Type character from tag string: 'i', 'f', 's', ... */
static char osc_type(void)
{
    return r_buf[osc_tag_pos() + 1];
}

/* Position of the first data byte after the type tag. */
static int osc_data_pos(void)
{
    int p = osc_tag_pos();
    int taglen = strlen(r_buf + p);
    return (p + taglen + 4) & ~3;
}

/* Read big-endian int from data region. */
static int osc_get_int(void)
{
    int dp = osc_data_pos();
    unsigned char *b = (unsigned char *)(r_buf + dp);
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

/* Read big-endian float from data region. */
static float osc_get_float(void)
{
    int v = osc_get_int();
    float f;
    memcpy(&f, &v, 4);
    return f;
}

/* Read string from data region. */
static const char *osc_get_str(void)
{
    return r_buf + osc_data_pos();
}

/* ── OSC reply builders ──────────────────────────────────────────────────── */

static void reply_i(const char *path, int v)
{
    s_len = Xsprint(s_buf, 0,      's', (void *)path);
    s_len = Xsprint(s_buf, s_len,  's', ",i");
    s_len = Xsprint(s_buf, s_len,  'i', &v);
}

static void reply_f(const char *path, float f)
{
    s_len = Xsprint(s_buf, 0,      's', (void *)path);
    s_len = Xsprint(s_buf, s_len,  's', ",f");
    s_len = Xsprint(s_buf, s_len,  'f', &f);
}

static void reply_s(const char *path, const char *str)
{
    s_len = Xsprint(s_buf, 0,      's', (void *)path);
    s_len = Xsprint(s_buf, s_len,  's', ",s");
    s_len = Xsprint(s_buf, s_len,  's', (void *)str);
}

/* Echo the incoming packet back verbatim (used for SET broadcast). */
static void echo_and_broadcast(void)
{
    memcpy(s_buf, r_buf, r_len);
    s_len = r_len;
    xbroadcast();
}

/* ── command handlers ───────────────────────────────────────────────────── */

/* /xinfo — identity handshake required by XAir_Command connect loop */
static void handle_xinfo(void)
{
    s_len = Xsprint(s_buf, 0,     's', "/xinfo");
    s_len = Xsprint(s_buf, s_len, 's', ",ssss");
    s_len = Xsprint(s_buf, s_len, 's', Xip_str);
    s_len = Xsprint(s_buf, s_len, 's', XNAME);
    s_len = Xsprint(s_buf, s_len, 's', XMODEL);
    s_len = Xsprint(s_buf, s_len, 's', XVERSION);
    xsend_current();
    if (Xverbose) printf("<- /xinfo  ip=%s model=%s ver=%s\n",
                         Xip_str, XMODEL, XVERSION);
}

/* /info — minimal echo */
static void handle_info(void)
{
    s_len = Xsprint(s_buf, 0, 's', "/info");
    xsend_current();
    if (Xverbose) printf("<- /info\n");
}

static void handle_xremote(void)
{
    client_refresh();
}

static void handle_meters(void)
{
    /* /meters ,siii "/meters/6" ch 0 0  —  just register sender */
    client_refresh();
    if (Xverbose) printf("   /meters subscribe\n");
}

/* ── generic param get/set ──────────────────────────────────────────────── */

/*
 * Infer a sensible default type for a path we haven't seen before.
 * Rules derived from XR18 OSC convention:
 *   /name, /label      → string
 *   /on, /invert, /hpon, /type, /icon, /color, /source, /mode  → int
 *   everything else    → float
 */
static char default_type_for(const char *path)
{
    const char *leaf = strrchr(path, '/');
    if (!leaf) return 'f';
    leaf++;
    if (strcmp(leaf, "name")   == 0 ||
        strcmp(leaf, "label")  == 0) return 's';
    if (strcmp(leaf, "on")     == 0 ||
        strcmp(leaf, "invert") == 0 ||
        strcmp(leaf, "hpon")   == 0 ||
        strcmp(leaf, "type")   == 0 ||
        strcmp(leaf, "icon")   == 0 ||
        strcmp(leaf, "color")  == 0 ||
        strcmp(leaf, "source") == 0 ||
        strcmp(leaf, "mode")   == 0 ||
        strcmp(leaf, "tap")    == 0) return 'i';
    return 'f';
}

static void handle_param(void)
{
    const char *path = r_buf;

    if (osc_is_set()) {
        /* SET: store value and broadcast */
        char t = osc_type();
        Param *p = param_get_or_create(path);
        if (!p) return;
        p->type = t;
        switch (t) {
        case 'i': p->v.i = osc_get_int();   break;
        case 'f': p->v.f = osc_get_float(); break;
        case 's': strncpy(p->v.s, osc_get_str(), sizeof(p->v.s) - 1); break;
        default:  break;
        }
        if (Xverbose) {
            switch (t) {
            case 'i': printf("   SET %s = %d\n",   path, p->v.i); break;
            case 'f': printf("   SET %s = %.4f\n", path, p->v.f); break;
            case 's': printf("   SET %s = \"%s\"\n", path, p->v.s); break;
            }
        }
        echo_and_broadcast();
    } else {
        /* GET: look up stored param and reply */
        Param *p = param_find(path);
        char   t = p ? p->type : default_type_for(path);

        if (Xverbose) printf("   GET %s (type=%c)\n", path, t);

        switch (t) {
        case 'i':
            reply_i(path, p ? p->v.i : 0);
            break;
        case 'f':
            reply_f(path, p ? p->v.f : 0.f);
            break;
        case 's':
            reply_s(path, p ? p->v.s : "");
            break;
        default:
            reply_f(path, 0.f);
            break;
        }
        xsend_current();
    }
}

/* ── local IP discovery ─────────────────────────────────────────────────── */

static void get_local_ip(void)
{
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) != 0) { strcpy(Xip_str, "127.0.0.1"); return; }
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *s = (struct sockaddr_in *)p->ifa_addr;
        char *a = inet_ntoa(s->sin_addr);
        if (strcmp(a, "127.0.0.1") == 0) continue;
        strncpy(Xip_str, a, sizeof(Xip_str) - 1);
        break;
    }
    freeifaddrs(ifa);
    if (!Xip_str[0]) strcpy(Xip_str, "127.0.0.1");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "i:v:l:f:h")) != -1) {
        switch (opt) {
        case 'i': strncpy(Xip_str, optarg, sizeof(Xip_str) - 1); break;
        case 'v': Xverbose          = atoi(optarg);  break;
        case 'l': sim_level         = atof(optarg);  break;
        case 'f': fx_type_default   = atoi(optarg);  break;
        default:
        case 'h':
            printf("usage: XR18 [-i ip] [-v 0|1] [-l level 0.0-1.0] [-f fx_type]\n");
            return 0;
        }
    }

    if (!Xip_str[0]) get_local_ip();

    /* Override FX type defaults if -f was given */
    {
        char path[32];
        int i;
        for (i = 1; i <= 4; i++) {
            sprintf(path, "/fx/%d/type", i);
            param_set_i(path, fx_type_default);
        }
    }

    init_defaults();

    if ((Xfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket"); return 1;
    }
    {
        int reuse = 1;
        setsockopt(Xfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    memset(&server_ip, 0, sizeof(server_ip));
    server_ip.sin_family      = AF_INET;
    server_ip.sin_addr.s_addr = INADDR_ANY;
    server_ip.sin_port        = htons(PORT);

    if (bind(Xfd, (struct sockaddr *)&server_ip, sizeof(server_ip)) < 0) {
        perror("bind"); return 1;
    }

    printf("XR18 emulator  %s  port %d  (level=%.2f, fx_type=%d)\n",
           Xip_str, PORT, sim_level, fx_type_default);

    fd_set         readfds;
    struct timeval timeout;
    struct timeval meter_next = {0, 0};

    while (keep_on) {
        /* push meter data to all subscribed clients every 60 ms */
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > meter_next.tv_sec ||
            (now.tv_sec == meter_next.tv_sec && now.tv_usec >= meter_next.tv_usec)) {
            time_t ts = time(NULL);
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].expire && ts < clients[i].expire) {
                    s_len = build_meter_packet(s_buf, sim_level);
                    sendto(Xfd, s_buf, s_len, 0,
                           &clients[i].addr, sizeof(struct sockaddr));
                }
            }
            meter_next = now;
            meter_next.tv_usec += 60000;
            if (meter_next.tv_usec >= 1000000) {
                meter_next.tv_sec++;
                meter_next.tv_usec -= 1000000;
            }
        }

        FD_ZERO(&readfds);
        FD_SET(Xfd, &readfds);
        timeout.tv_sec  = 0;
        timeout.tv_usec = 10000;    /* 10 ms poll */
        if (select(Xfd + 1, &readfds, NULL, NULL, &timeout) <= 0) continue;

        r_len = recvfrom(Xfd, r_buf, BSIZE - 1, 0,
                         (struct sockaddr *)&client_ip, &client_ip_len);
        if (r_len <= 0) continue;
        r_buf[r_len] = 0;

        if (Xverbose) printf("-> %s\n", r_buf);

        if      (strcmp(r_buf, "/xinfo")  == 0 ||
                 strcmp(r_buf, "/status") == 0)    handle_xinfo();
        else if (strcmp(r_buf, "/info")   == 0)    handle_info();
        else if (strcmp(r_buf, "/xremote")== 0)    handle_xremote();
        else if (strncmp(r_buf, "/meters", 7) == 0) handle_meters();
        else if (r_buf[0] == '/')                  handle_param();
    }

    close(Xfd);
    return 0;
}
