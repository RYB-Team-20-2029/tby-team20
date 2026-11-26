/* Purpose: RYB Comms frame codec: the 13-bit packet layout, even parity and the receive checks.
 * Owning module: management (mgmt/). Pure C with no OS headers, so it builds and tests anywhere. */

#ifndef RYB_PACKET_H
#define RYB_PACKET_H

#include <stdint.h>

#define RYB_FRAME_BITS 13u
#define RYB_FRAME_MASK 0x1FFFu

/* Sender addresses, from the guideline's field table. */
#define RYB_ADDR_HEARTBEAT 0u
#define RYB_ADDR_SOUND     1u
#define RYB_ADDR_DECISION  2u
#define RYB_ADDR_MOTOR     3u
#define RYB_ADDR_MAX       3u

#define RYB_SEGMENT_MAX 31u

/* Wire values of the 2-bit trend field. Code 3 (binary 11) is reserved: an encoder never emits it
 * and a receiver treats an appended packet carrying it as malformed. */
typedef enum {
    RYB_TREND_INCREASING = 0,
    RYB_TREND_DECREASING = 1,
    RYB_TREND_NA = 2
} ryb_trend_t;

/* RYB_OK is zero and every error is negative, so `rc != RYB_OK` and `rc < 0` read the same. */
typedef enum {
    RYB_OK = 0,
    RYB_ERR_PARAM = -1,   /* argument out of range, or a null pointer        */
    RYB_ERR_PARITY = -2,  /* received frame's 13 bits do not XOR to 0        */
    RYB_ERR_ADDRESS = -3, /* sender is not one expected on that link         */
    RYB_ERR_TREND = -4,   /* appended packet carries the reserved trend 11   */
    RYB_ERR_IO = -5,      /* /dev/mem could not be opened or mapped          */
    RYB_ERR_BUSY = -6,    /* transmit queue full, the line is saturated      */
    RYB_ERR_STATE = -7    /* not initialised, or no outgoing link configured */
} ryb_status_t;

typedef enum {
    RYB_PACKET_APPENDED = 0,
    RYB_PACKET_COMMAND = 1
} ryb_packet_kind_t;

typedef struct {
    uint8_t sender;
    ryb_packet_kind_t kind;
    union {
        struct {
            uint8_t data;
            ryb_trend_t trend;
        } appended;
        struct {
            uint8_t first;
            uint8_t second;
        } command;
    } u;
} ryb_packet_t;

/* Even-parity bit for `bits`: 1 when they hold an odd number of 1s. */
uint8_t ryb_parity(uint16_t bits);

/* Build a frame. Parity is always computed here and is never a parameter. An out-of-range
 * argument is rejected with RYB_ERR_PARAM and *frame is left untouched. */
ryb_status_t ryb_pack_appended(uint8_t addr, uint8_t data, ryb_trend_t trend, uint16_t* frame);
ryb_status_t ryb_pack_command(uint8_t addr, uint16_t first, uint16_t second, uint16_t* frame);

uint8_t ryb_frame_sender(uint16_t frame);

/* Run the guideline's receive checks on a 13-bit frame, in its order, and decode it.
 * accept_mask has bit n set for each sender address n expected on the link. Frames from
 * command_addr decode as command packets, all others as appended packets.
 * *pkt is only meaningful when RYB_OK is returned. */
ryb_status_t ryb_frame_decode(uint16_t frame, uint8_t accept_mask, uint8_t command_addr,
                              ryb_packet_t* pkt);

#endif /* RYB_PACKET_H */
