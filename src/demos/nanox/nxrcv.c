#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <nano-X.h>
#include "nxproto.h"

/* ---------- Nano-X globals ---------- */

static GR_WINDOW_ID win;
static GR_GC_ID gc;
static int sockfd;

/* ---------- 16-color palette (must match sender) ---------- */

static GR_COLOR color_from_index[16] = {
    GR_RGB(0,0,0),
    GR_RGB(128,0,0),
    GR_RGB(0,128,0),
    GR_RGB(128,128,0),
    GR_RGB(0,0,128),
    GR_RGB(128,0,128),
    GR_RGB(0,128,128),
    GR_RGB(192,192,192),
    GR_RGB(128,128,128),
    GR_RGB(255,0,0),
    GR_RGB(0,255,0),
    GR_RGB(255,255,0),
    GR_RGB(0,0,255),
    GR_RGB(255,0,255),
    GR_RGB(0,255,255),
    GR_RGB(255,255,255)
};

/* ----------------------------------------------------------- */
/* EmuGrArea for indexed strips                                */
/* ----------------------------------------------------------- */
static void EmuGrArea(GR_WINDOW_ID wid, GR_GC_ID gc,
                      GR_COORD x, GR_COORD y,
                      GR_SIZE w, GR_SIZE h,
                      const unsigned char *buf,
                      GR_SIZE pitch)
{
    if (w == 0 || h == 0)
        return;

    for (int iy = 0; iy < (int)h; iy++) {
        const unsigned char *row = buf + iy * pitch;

        int run_start = 0;
        unsigned char run_idx = row[0];

        for (int ix = 1; ix < (int)w; ix++) {
            if (row[ix] != run_idx) {
                GrSetGCForeground(gc, color_from_index[run_idx]);
                GrLine(wid, gc,
                       x + run_start, y + iy,
                       x + ix - 1,    y + iy);

                run_start = ix;
                run_idx = row[ix];
            }
        }

        GrSetGCForeground(gc, color_from_index[run_idx]);
        GrLine(wid, gc,
               x + run_start,  y + iy,
               x + (int)w - 1, y + iy);
    }
}

/* ---------- Networking helpers ---------- */

static void recvn(void *buf, int len)
{
    int r, off = 0;
    while (off < len) {
        r = read(sockfd, (char *)buf + off, len - off);
        if (r <= 0) {
            perror("read");
            exit(1);
        }
        off += r;
    }
}

static void connect_to_server(const char *ip, int port)
{
    struct sockaddr_in sa;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_aton(ip, &sa.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("connect");
        exit(1);
    }
}

/* ---------- Frame handling ---------- */

static void handle_frame(void)
{
    static unsigned char packed[(NX_W * NX_H) / 2];
    static unsigned char indexbuf[NX_W * NX_H];
    int x, y;

    recvn(packed, sizeof(packed));

    /* unpack 4bpp → 8bpp indexed buffer */
    for (y = 0; y < NX_H; y++) {
        for (x = 0; x < NX_W; x++) {
            unsigned char b = packed[(y * NX_W + x) / 2];
            indexbuf[y * NX_W + x] =
                (x & 1) ? (b & 0x0F) : (b >> 4);
        }
    }

    EmuGrArea(win, gc,
              0, 0,
              NX_W, NX_H,
              indexbuf,
              NX_W);
}

/* ---------- Empty input handlers ---------- */

static void handle_mouse(GR_EVENT *ev) { (void)ev; }
static void handle_key(GR_EVENT *ev)   { (void)ev; }

/* ---------- Main ---------- */

int main(int argc, char **argv)
{
    GR_EVENT ev;
    nx_frame_hdr_t hdr;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <server-ip> <port>\n", argv[0]);
        return 1;
    }

    GrOpen();
    gc = GrNewGC();

    win = GrNewWindow(GR_ROOT_WINDOW_ID,
                      10, 10,
                      NX_W, NX_H,
                      1,
                      GR_RGB(128,128,128),
                      GR_RGB(0,0,0));

    GrSelectEvents(win,
        GR_EVENT_MASK_EXPOSURE |
        GR_EVENT_MASK_BUTTON_DOWN |
        GR_EVENT_MASK_KEY_DOWN |
        GR_EVENT_MASK_CLOSE_REQ);

    GrMapWindow(win);

    connect_to_server(argv[1], atoi(argv[2]));

    for (;;) {

        /* receive one frame header */
        recvn(&hdr, sizeof(hdr));

        if (hdr.type == NX_MSG_FRAME) {
            handle_frame();
        }

        /* process Nano-X events */
        while (GrPeekEvent(&ev)) {
            GrGetNextEvent(&ev);

            if (ev.type == GR_EVENT_TYPE_BUTTON_DOWN)
                handle_mouse(&ev);
            else if (ev.type == GR_EVENT_TYPE_KEY_DOWN)
                handle_key(&ev);
            else if (ev.type == GR_EVENT_TYPE_CLOSE_REQ)
                exit(0);
        }
    }

    return 0;
}

