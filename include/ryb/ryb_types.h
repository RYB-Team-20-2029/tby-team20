/* Purpose: shared enums and return codes used across all four RYB submodules.
 * Owning module: management (mgmt/) — see docs/interface-contract.md. */

#ifndef RYB_TYPES_H
#define RYB_TYPES_H

#include <stdint.h>

/* Direction of change of a sensed quantity over the module's own trend window.
 * The numeric values are wire-visible: ryb_ipc.h carries them as uint8_t.
 * Do NOT renumber without bumping RYB_IPC_VERSION. */
typedef enum {
    RYB_TREND_FALLING = 0,
    RYB_TREND_STABLE  = 1,
    RYB_TREND_RISING  = 2
} ryb_trend_t;

/* Common return code for every function that can fail.
 * Convention: RYB_OK is zero, all errors are negative, so `if (rc != RYB_OK)`
 * and `if (rc < 0)` are equivalent and both read correctly. */
typedef enum {
    RYB_OK          =  0,  /* success                                        */
    RYB_ERR_PARAM   = -1,  /* caller passed an out-of-range or null argument  */
    RYB_ERR_RANGE   = -2,  /* value computed but outside its declared bounds  */
    RYB_ERR_TIMEOUT = -3,  /* expected event did not arrive in time           */
    RYB_ERR_NOLOCK  = -4,  /* sensor has no usable signal yet (warm-up/lost)  */
    RYB_ERR_HW      = -5,  /* peripheral reported a fault                     */
    RYB_ERR_IPC     = -6   /* backbone framing, version or checksum mismatch  */
} ryb_status_t;

/* Which submodule a message came from. Wire-visible; see ryb_ipc.h. */
typedef enum {
    RYB_NODE_HEARTBEAT = 1,
    RYB_NODE_SOUND     = 2,
    RYB_NODE_DECISION  = 3,
    RYB_NODE_MOTOR     = 4
} ryb_node_t;

#endif /* RYB_TYPES_H */
