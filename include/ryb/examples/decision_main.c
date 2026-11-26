/* Purpose: example main.c for the decision module: take BPM and loudness, command the motor.
 * Dummy decision rule. Owning module: decision. Copy to modules/decision/src/main.c. */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "ryb/ryb.h"

/* The latest readings, written by the callbacks and read by the main loop. Both run on the same
 * thread (inside ryb_update()), so plain variables are fine. */
static uint8_t bpm;
static uint8_t volume;
static int have_bpm;
static int have_volume;

/* Called by ryb_update() for each heartbeat packet (sound passes them on). */
void dec_receive_heartbeat(uint8_t value, ryb_trend_t trend)
{
    (void) trend;
    bpm = value;
    have_bpm = 1;
}

/* Called by ryb_update() for each sound packet. */
void dec_receive_sound(uint8_t value, ryb_trend_t trend)
{
    (void) trend;
    volume = value;
    have_volume = 1;
}

/* DUMMY: stands in for the stress-matrix navigation. Maps the readings to regions 1..5. */
static void choose_regions(uint16_t* f_region, uint16_t* a_region)
{
    *f_region = (uint16_t) (1u + (bpm >= 60u ? (bpm - 60u) / 30u : 0u) % 5u);
    *a_region = (uint16_t) (1u + volume / 21u % 5u);
}

int main(void)
{
    if (ryb_begin() != RYB_OK) {
        return 1;
    }

    unsigned loops = 0;
    for (;;) {
        ryb_update(); /* runs the two callbacks above for whatever arrived */

        /* Twice a second, once both sensors have reported: command the motor. Sending the same
         * command again is fine, and covers a packet the line lost. */
        if (loops++ % 50u == 0u && have_bpm && have_volume) {
            uint16_t f_region;
            uint16_t a_region;
            choose_regions(&f_region, &a_region);
            ryb_send_command(f_region, a_region); /* first = frequency, second = amplitude */
            printf("bpm %u volume %u -> F%u A%u\n", (unsigned) bpm, (unsigned) volume,
                   (unsigned) f_region, (unsigned) a_region);
        }

        usleep(10000);
    }
}
