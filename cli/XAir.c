//
// XAir.c — Minimal XR18/XAir emulator for testing
//
// Listens on port 10024 and handles the subset of OSC commands
// used by XTap and related tools:
//
//   /info              → responds with /info (connection handshake)
//   /xremote           → registers client for meter pushes
//   /meters ,siii ...  → registers client for /meters/6 data
//   /fx/N/type         → responds with FX type (default: DLY=10)
//   /fx/N/par/02 ,f v  → accepts tap tempo set, echoes back
//
// Usage: XAir [-i ip] [-v 0|1] [-l level] [-f fx_type]
//   -l  simulated meter level, 0.0–1.0 (default 0.0; use >0 to trigger auto-tap)
//   -f  FX type to report for all slots (default 10 = DLY)
//

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

#define PORT   10024
#define BSIZE  512

static int     Xfd, Xverbose = 1;
static char    Xip_str[32] = "";
static float   sim_level   = 0.0f;
static int     fx_type     = 10;   /* 10 = DLY */
static int     keep_on     = 1;

static struct sockaddr_in  server_ip, client_ip;
static socklen_t           client_ip_len = sizeof(client_ip);
static char r_buf[BSIZE], s_buf[BSIZE];
static int  r_len, s_len;

/* registered meter subscriber */
static struct sockaddr meter_client;
static int             meter_client_valid = 0;
static int             meter_channel      = 0;   /* 0-indexed */

/* ── OSC helpers ────────────────────────────────────────────────────────── */

/* Build /meters/6 blob packet with sim_level at the gate-meter offset (28).
 * Packet layout matches what X32TapW sends and XTap parses. */
static int build_meter_packet(char *buf, float level) {
    int idx = 0;
    /* address: /meters/6 */
    memset(buf, 0, 64);
    memcpy(buf, "/meters/6", 9);
    idx = 12;   /* padded to 12 */
    /* type tag: ,b (blob) */
    buf[idx++] = ','; buf[idx++] = 'b';
    idx = 16;
    /* blob size (big-endian int): 40 bytes */
    buf[16] = 0; buf[17] = 0; buf[18] = 0; buf[19] = 40;
    idx = 20;
    /* blob data: zeros except gate meter float at offset 28 from start of packet */
    memcpy(buf + 28, &level, 4);   /* little-endian float — XR matches host byte order */
    return 60;
}

/* ── send helper ────────────────────────────────────────────────────────── */

static void xsend(const struct sockaddr *to, socklen_t len) {
    sendto(Xfd, s_buf, s_len, 0, to, len);
}

/* ── command handlers ───────────────────────────────────────────────────── */

static void handle_info(void) {
    /* echo /info back so client knows we're here */
    s_len = Xsprint(s_buf, 0, 's', "/info");
    xsend((struct sockaddr *)&client_ip, client_ip_len);
    if (Xverbose) printf("<- /info\n");
}

static void handle_xremote(void) {
    /* register this client for meter updates */
    meter_client       = *(struct sockaddr *)&client_ip;
    meter_client_valid = 1;
    if (Xverbose) printf("   /xremote registered %s\n",
                         inet_ntoa(client_ip.sin_addr));
}

static void handle_meters(void) {
    /* /meters ,siii "/meters/6" channel 0 0
     * byte 31 carries the channel index */
    if (r_len >= 32) {
        meter_channel      = (unsigned char)r_buf[31];
        meter_client       = *(struct sockaddr *)&client_ip;
        meter_client_valid = 1;
        if (Xverbose) printf("   /meters subscribe ch=%d\n", meter_channel + 1);
    }
}

static void handle_fx_type(int slot) {
    char path[16];
    sprintf(path, "/fx/%d/type", slot);
    /* respond: address, type tag ,i, big-endian int */
    s_len = Xsprint(s_buf, 0, 's', path);
    s_len = Xsprint(s_buf, s_len, 's', ",i");
    /* big-endian fx_type */
    s_buf[s_len+0] = 0;
    s_buf[s_len+1] = 0;
    s_buf[s_len+2] = 0;
    s_buf[s_len+3] = (char)fx_type;
    s_len += 4;
    xsend((struct sockaddr *)&client_ip, client_ip_len);
    if (Xverbose) printf("<- %s = %d\n", path, fx_type);
}

static void handle_fx_par(int slot) {
    /* echo the set command back to caller (X32 behaviour) */
    memcpy(s_buf, r_buf, r_len);
    s_len = r_len;
    xsend((struct sockaddr *)&client_ip, client_ip_len);
    if (Xverbose) {
        float f;
        memcpy(&f, r_buf + r_len - 4, 4);   /* last 4 bytes are the float, big-endian */
        /* convert from big-endian */
        char tmp[4] = { r_buf[r_len-1], r_buf[r_len-2], r_buf[r_len-3], r_buf[r_len-4] };
        memcpy(&f, tmp, 4);
        printf("   /fx/%d/par set %.4f  (%d ms)\n", slot, f, (int)(f * 3000));
    }
}

/* ── get local IP ───────────────────────────────────────────────────────── */

static void get_local_ip(void) {
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) != 0) { strcpy(Xip_str, "127.0.0.1"); return; }
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *s = (struct sockaddr_in *)p->ifa_addr;
        char *a = inet_ntoa(s->sin_addr);
        if (strcmp(a, "127.0.0.1") == 0) continue;
        strncpy(Xip_str, a, sizeof(Xip_str)-1);
        break;
    }
    freeifaddrs(ifa);
    if (!Xip_str[0]) strcpy(Xip_str, "127.0.0.1");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "i:v:l:f:h")) != -1) {
        switch (opt) {
        case 'i': strncpy(Xip_str, optarg, sizeof(Xip_str)-1); break;
        case 'v': Xverbose = atoi(optarg); break;
        case 'l': sim_level = atof(optarg); break;
        case 'f': fx_type   = atoi(optarg); break;
        default:
        case 'h':
            printf("usage: XAir [-i ip] [-v 0|1] [-l level 0.0-1.0] [-f fx_type]\n");
            return 0;
        }
    }

    if (!Xip_str[0]) get_local_ip();

    if ((Xfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket"); return 1;
    }
    int reuse = 1;
    setsockopt(Xfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&server_ip, 0, sizeof(server_ip));
    server_ip.sin_family      = AF_INET;
    server_ip.sin_addr.s_addr = INADDR_ANY;
    server_ip.sin_port        = htons(PORT);

    if (bind(Xfd, (struct sockaddr *)&server_ip, sizeof(server_ip)) < 0) {
        perror("bind"); return 1;
    }
    printf("XAir emulator - listening on %s:%d  (level=%.2f, fx_type=%d)\n",
           Xip_str, PORT, sim_level, fx_type);

    fd_set readfds;
    struct timeval timeout;
    struct timeval meter_next = {0, 0};

    while (keep_on) {
        /* send meter data to subscribed client every 60ms */
        struct timeval now;
        gettimeofday(&now, NULL);
        if (meter_client_valid &&
            (now.tv_sec > meter_next.tv_sec ||
             (now.tv_sec == meter_next.tv_sec && now.tv_usec >= meter_next.tv_usec))) {
            s_len = build_meter_packet(s_buf, sim_level);
            sendto(Xfd, s_buf, s_len, 0, &meter_client, sizeof(struct sockaddr));
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
        timeout.tv_usec = 10000;   /* 10ms poll */
        if (select(Xfd + 1, &readfds, NULL, NULL, &timeout) <= 0) continue;

        r_len = recvfrom(Xfd, r_buf, BSIZE, 0,
                         (struct sockaddr *)&client_ip, &client_ip_len);
        if (r_len <= 0) continue;
        r_buf[r_len] = 0;

        if (Xverbose) printf("-> %s\n", r_buf);

        if      (strcmp(r_buf, "/info")    == 0) handle_info();
        else if (strcmp(r_buf, "/xremote") == 0) handle_xremote();
        else if (strncmp(r_buf, "/meters", 7) == 0) handle_meters();
        else if (strncmp(r_buf, "/fx/", 4) == 0) {
            /* /fx/N/type or /fx/N/par/02 */
            int slot = 0;
            sscanf(r_buf + 4, "%d", &slot);
            if (strstr(r_buf, "/type")) handle_fx_type(slot);
            else if (strstr(r_buf, "/par")) handle_fx_par(slot);
        }
    }
    close(Xfd);
    return 0;
}
