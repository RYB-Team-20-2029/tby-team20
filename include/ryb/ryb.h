/* Purpose: the one header a module includes: start, update and stop RYB Comms and the LCD.
 * Owning module: management (mgmt/). See include/ryb/README.md, "Writing a module". */

#ifndef RYB_H
#define RYB_H

#include <stdint.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_lcd.h"

/* Start the comms (pins and timing from ryb_comms.json) and the LCD. A missing LCD is reported
 * and skipped. Returns RYB_OK, or the comms error; run as root on the PYNQ. */
ryb_status_t ryb_begin(void);

/* Call this every pass of the main loop, at least every few milliseconds. It runs this module's
 * receive callbacks for every packet that arrived since the last call, and redraws the LCD five
 * times a second. Returns how many packets it delivered. */
int ryb_update(void);

/* Stop the comms and release the line and the LCD. Also happens at exit(). */
void ryb_end(void);

/* The trend field for a new reading: up, down, or n/a when there is no earlier reading (pass a
 * negative `previous`) or the value did not change. */
static inline ryb_trend_t ryb_trend(int previous, int current)
{
    if (previous < 0 || current == previous) {
        return RYB_TREND_NA;
    }
    return current > previous ? RYB_TREND_INCREASING : RYB_TREND_DECREASING;
}

#endif /* RYB_H */
