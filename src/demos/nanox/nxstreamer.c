#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>

#include <nano-X.h>
#include "nxproto.h"

static GR_GC_ID gc;
static GR_PIXMAP_ID pm;
static int sockfd;

/* crude grayscale quantization to 16 colors */
static uint8_t quantize(GR_PIXELVAL p)
{
    uint8_t r = (p >> 16) & 0xFF;
    return r >> 4;
}

static void wait_for_client(int port)
{
    int s;
    struct sockaddr_in sa;

    s = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&sa, sizeof(sa));
    listen(s, 1);

    sockfd = accept(s, NULL, NULL);
}

static void draw_clock(void)
{
    char buf[64];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    snprintf(buf, sizeof(buf),
             "%02d:%02d:%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    GrSetGCForeground(gc, GR_RGB(0,0,0));
    GrFillRect(pm, gc, 0, 0, NX_W, NX_H);

    GrSetGCForeground(gc, GR_RGB(255,255,255));
    GrText(pm, gc, 60, 100, buf, strlen(buf), GR_TFASCII);
}

static void capture_and_send(void)
{
    static GR_PIXELVAL pixels[NX_W * NX_H];
    static uint8_t packed[(NX_W * NX_H) / 2];
    nx_frame_hdr_t hdr;
    int i;

    GrReadArea(pm, 0, 0, NX_W, NX_H, pixels);

    for (i = 0; i < NX_W * NX_H; i += 2) {
        packed[i/2] =
            (quantize(pixels[i]) << 4) |
             quantize(pixels[i+1]);
    }

    hdr.type = NX_MSG_FRAME;
    hdr.x = 0;
    hdr.y = 0;
    hdr.w = NX_W;
    hdr.h = NX_H;

    write(sockfd, &hdr, sizeof(hdr));
    write(sockfd, packed, sizeof(packed));
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    GrOpen();
    gc = GrNewGC();
    pm = GrNewPixmap(NX_W, NX_H, NULL);

    wait_for_client(atoi(argv[1]));

    while (1) {
        draw_clock();
        capture_and_send();
        sleep(1);
    }
}

