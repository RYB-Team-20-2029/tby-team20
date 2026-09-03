/* Purpose: authoritative wire format for all traffic on the supplied backbone.
 * Owning module: management (mgmt/) — any edit is a PR that every affected
 * module owner must acknowledge before merge. See docs/interface-contract.md. */

#ifndef RYB_IPC_H
#define RYB_IPC_H

#include <stdint.h>

#define RYB_IPC_VERSION 0x0001 /* bump on any layout change */

/* Bounds that the wire format guarantees. Producers clamp, consumers may assert.
 * The BPM range itself lives in ryb_config.h as RYB_HEART_BPM_MIN/MAX and is
 * deliberately not repeated here — one value, one definition. Only the wire
 * sentinel belongs to the protocol. */
#define RYB_BPM_NO_LOCK    0u /* sentinel: sensor has no usable signal */
#define RYB_LOUDNESS_MIN   0u
#define RYB_LOUDNESS_MAX 100u
#define RYB_REGION_MIN     1u
#define RYB_REGION_MAX     5u

/* trend fields below carry the numeric values of ryb_trend_t (ryb_types.h):
 * 0 falling, 1 stable, 2 rising. They are declared uint8_t, not the enum,
 * because enum width is implementation-defined and this crosses the wire. */

/* heartbeat -> decision */
typedef struct {
    uint16_t bpm;   /* 60..240; 0 = no lock          */
    uint8_t trend;  /* 0 falling, 1 stable, 2 rising */
    uint8_t valid;  /* 0 = ignore this sample        */
} ryb_heart_msg_t;

/* sound -> decision */
typedef struct {
    uint8_t loudness; /* 0..100 percent                */
    uint8_t trend;    /* 0 falling, 1 stable, 2 rising */
    uint8_t valid;    /* 0 = ignore this sample        */
} ryb_sound_msg_t;

/* decision -> motor */
typedef struct {
    uint8_t f_region; /* 1..5, rocking frequency region */
    uint8_t a_region; /* 1..5, rocking amplitude region */
} ryb_motion_cmd_t;

/* TODO(mgmt): the framing that wraps these payloads on the backbone is not yet
 * decided — physical link type, byte order, whether a header carries
 * RYB_IPC_VERSION and a sender id (ryb_node_t), and whether a checksum is
 * needed. Tracked as open questions in docs/interface-contract.md. Nothing is
 * declared here until the four module owners agree, because a guess that ships
 * is a guess that four boards then have to un-ship. */

#endif /* RYB_IPC_H */
