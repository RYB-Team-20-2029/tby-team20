/* Purpose: example main.c for the heartbeat module: measure BPM, send it on. Dummy sensor.
 * Owning module: heartbeat. Copy to modules/heartbeat/src/main.c and replace the dummy parts. */

/* Heartbeat starts the chain: it only sends, so it defines no receive callbacks. */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "ryb/ryb.h"

/* DUMMY: stands in for the real BPM measurement (src/app/ in the real module). */
static uint8_t read_bpm(void)
{
    static int bpm = 70;
    bpm = bpm >= 120 ? 70 : bpm + 5;
    return (uint8_t) bpm;
}

int main(void)
{
    if (ryb_begin() != RYB_OK) {
        return 1;
    }

    int previous = -1; /* no reading yet, so the first trend is n/a */
    unsigned loops = 0;
    for (;;) {
        ryb_update(); /* keeps the LCD current */

        /* Once a second: a new reading goes out. The main loop runs every 10 ms. */
        if (loops++ % 100u == 0u) {
            const uint8_t bpm = read_bpm();
            ryb_send_appended(bpm, ryb_trend(previous, bpm));
            printf("sent bpm %u\n", (unsigned) bpm);
            previous = bpm;
        }

        usleep(10000);
    }
}
