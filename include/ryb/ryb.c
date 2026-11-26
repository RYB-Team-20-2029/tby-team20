/* Purpose: ryb_begin(), ryb_update() and ryb_end(): the comms and LCD together, for modules.
 * Owning module: management (mgmt/). */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "ryb/ryb.h"

#define SCREEN_PERIOD_MS 200u

static bool lcd_on;
static uint64_t next_screen_ms;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

ryb_status_t ryb_begin(void)
{
    const ryb_status_t rc = ryb_comms_init();
    if (rc != RYB_OK) {
        fprintf(stderr, "ryb_begin: comms did not start (%d); run as root on the PYNQ\n", (int) rc);
        return rc;
    }
    lcd_on = ryb_lcd_init() == RYB_OK;
    if (!lcd_on) {
        fprintf(stderr, "ryb_begin: no LCD, carrying on without it\n");
    }
    next_screen_ms = now_ms();
    return RYB_OK;
}

int ryb_update(void)
{
    const int delivered = ryb_comms_poll();
    if (lcd_on && now_ms() >= next_screen_ms) {
        ryb_lcd_show_comms();
        next_screen_ms = now_ms() + SCREEN_PERIOD_MS;
    }
    return delivered;
}

void ryb_end(void)
{
    ryb_comms_shutdown();
    if (lcd_on) {
        ryb_lcd_close();
        lcd_on = false;
    }
}
