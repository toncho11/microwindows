#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <nano-X.h>
#include "nxproto.h"

static GR_WINDOW_ID win;
static GR_GC_ID gc;
static int sockfd;

static void recvn(void *buf, int len)
{
    int r, off = 0;
    while (off < len) {
        r = read(sockfd, (char *)buf + off, len - off);
        if (r <= 0) exit(1);
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

static void handle_frame(void)
{
    static uint8_t packed[(NX_W * NX_H) / 2];
    static GR_PIXELVAL line[NX_W];
    int x, y;

    recvn(packed, sizeof(packed));

    for (y = 0; y < NX_H; y++) {
        for (x = 0; x < NX_W; x++) {
            uint8_t byte = packed[(y * NX_W + x) / 2];
            line[x] = (x & 1) ? (byte & 0x0F) : (byte >> 4);
        }
        GrArea(win, gc, 0, y, NX_W, 1, line, GR_PIXELVALS);
    }
}

/* empty handlers */
static void handle_mouse(GR_EVENT *ev) { (void)ev; }
static void handle_key(GR_EVENT *ev)   { (void)ev; }

int main(int argc, char **argv)
{
    GR_EVENT ev;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <server-ip> <port>\n", argv[0]);
        return 1;
    }

    GrOpen();
    gc = GrNewGC();

    win = GrNewWindow(GR_ROOT_WINDOW_ID,
                      10, 10,
                      NX_W, NX_H,
                      1, GR_RGB(128,128,128), GR_RGB(0,0,0));

    GrSelectEvents(win,
        GR_EVENT_MASK_EXPOSURE |
        GR_EVENT_MASK_BUTTON_DOWN |
        GR_EVENT_MASK_KEY_DOWN |
        GR_EVENT_MASK_CLOSE_REQ);

    GrMapWindow(win);

    connect_to_server(argv[1], atoi(argv[2]));

    while (1) {
        nx_frame_hdr_t hdr;

        recvn(&hdr, sizeof(hdr));
        if (hdr.type == NX_MSG_FRAME)
            handle_frame();

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
}

