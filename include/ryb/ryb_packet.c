/* Purpose: RYB Comms frame codec: packing, parity and the receive checks.
 * Owning module: management (mgmt/). */

#include <stddef.h>
#include <stdint.h>

#include "ryb/ryb_packet.h"

/* Bit positions from the frame format tables. */
#define ADDR_SHIFT   11u
#define DATA_SHIFT   3u
#define TREND_SHIFT  1u
#define FIRST_SHIFT  6u
#define SECOND_SHIFT 1u

uint8_t ryb_parity(uint16_t bits)
{
    /* Each pass clears the lowest set bit and flips the accumulator, so no count is kept. */
    uint8_t parity = 0u;
    while (bits != 0u) {
        parity ^= 1u;
        bits &= (uint16_t) (bits - 1u);
    }
    return parity;
}

ryb_status_t ryb_pack_appended(uint8_t addr, uint8_t data, ryb_trend_t trend, uint16_t* frame)
{
    /* Reject rather than mask: a truncated address or trend would reach the wire as a different,
     * valid-looking packet. */
    if (frame == NULL || addr > RYB_ADDR_MAX || (unsigned) trend > (unsigned) RYB_TREND_NA) {
        return RYB_ERR_PARAM;
    }

    const uint16_t bits = (uint16_t) (((unsigned) addr << ADDR_SHIFT) |
                                      ((unsigned) data << DATA_SHIFT) |
                                      ((unsigned) trend << TREND_SHIFT));
    *frame = (uint16_t) (bits | ryb_parity(bits));
    return RYB_OK;
}

ryb_status_t ryb_pack_command(uint8_t addr, uint16_t first, uint16_t second, uint16_t* frame)
{
    if (frame == NULL || addr > RYB_ADDR_MAX || first > RYB_SEGMENT_MAX ||
        second > RYB_SEGMENT_MAX) {
        return RYB_ERR_PARAM;
    }

    /* (first << 5) | second lands in the same ten bits as an appended packet's data and trend,
     * which is what lets one receiver path clock in both types. */
    const uint16_t bits = (uint16_t) (((unsigned) addr << ADDR_SHIFT) |
                                      ((unsigned) first << FIRST_SHIFT) |
                                      ((unsigned) second << SECOND_SHIFT));
    *frame = (uint16_t) (bits | ryb_parity(bits));
    return RYB_OK;
}

uint8_t ryb_frame_sender(uint16_t frame)
{
    return (uint8_t) ((frame >> ADDR_SHIFT) & 0x3u);
}

ryb_status_t ryb_frame_decode(uint16_t frame, uint8_t accept_mask, uint8_t command_addr,
                              ryb_packet_t* pkt)
{
    /* Check 1, thirteen bits received, is the frame width itself: the bit loop always clocks in
     * exactly 13, so anything wider is a caller bug. */
    if (pkt == NULL || (frame & ~RYB_FRAME_MASK) != 0u) {
        return RYB_ERR_PARAM;
    }
    if (ryb_parity(frame) != 0u) {
        return RYB_ERR_PARITY;
    }

    const uint8_t sender = ryb_frame_sender(frame);
    if ((accept_mask & (1u << sender)) == 0u) {
        return RYB_ERR_ADDRESS;
    }

    if (sender == command_addr) {
        pkt->sender = sender;
        pkt->kind = RYB_PACKET_COMMAND;
        pkt->u.command.first = (uint8_t) ((frame >> FIRST_SHIFT) & 0x1Fu);
        pkt->u.command.second = (uint8_t) ((frame >> SECOND_SHIFT) & 0x1Fu);
        return RYB_OK;
    }

    const uint8_t trend = (uint8_t) ((frame >> TREND_SHIFT) & 0x3u);
    if (trend > (uint8_t) RYB_TREND_NA) {
        return RYB_ERR_TREND;
    }
    pkt->sender = sender;
    pkt->kind = RYB_PACKET_APPENDED;
    pkt->u.appended.data = (uint8_t) ((frame >> DATA_SHIFT) & 0xFFu);
    pkt->u.appended.trend = (ryb_trend_t) trend;
    return RYB_OK;
}
