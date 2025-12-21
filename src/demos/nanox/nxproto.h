#ifndef NXPROTO_H
#define NXPROTO_H

#include <stdint.h>

/* Fixed remote window size */
#define NX_W 300
#define NX_H 200

/* Message types */
#define NX_MSG_FRAME  1
#define NX_MSG_INPUT  2

/* Frame header */
typedef struct {
    uint8_t  type;   /* NX_MSG_FRAME */
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} nx_frame_hdr_t;

#endif

