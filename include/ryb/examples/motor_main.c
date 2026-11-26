/* Purpose: example main.c for the motor module: take decision's commands, drive the cradle.
 * Dummy PWM. Owning module: motor. Copy to modules/motor/src/main.c. */

/* Motor ends the chain: it only receives, so it never calls a send function. */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "ryb/ryb.h"

static uint8_t f_region = 1;
static uint8_t a_region = 1;
static int new_command;

/* Called by ryb_update() for each command packet from decision. */
void mot_receive_command(uint8_t first, uint8_t second)
{
    if (first < 1u || first > 5u || second < 1u || second > 5u) {
        return; /* a wire is not a promise: ignore regions outside 1..5 */
    }
    f_region = first;
    a_region = second;
    new_command = 1;
}

/* DUMMY: stands in for setting the two PWM duty cycles (src/hal/, with the 90 % clamp). */
static void drive_cradle(uint8_t f, uint8_t a)
{
    printf("drive: frequency region %u, amplitude region %u\n", (unsigned) f, (unsigned) a);
}

int main(void)
{
    if (ryb_begin() != RYB_OK) {
        return 1;
    }

    for (;;) {
        ryb_update(); /* runs mot_receive_command() for whatever arrived */

        if (new_command) {
            new_command = 0;
            drive_cradle(f_region, a_region);
        }

        usleep(10000);
    }
}
