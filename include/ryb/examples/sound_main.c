/* Purpose: example main.c for the sound module: measure loudness, send it on. Dummy microphone.
 * Owning module: sound. Copy to modules/sound/src/main.c and replace the dummy parts. */

/* Sound sits between heartbeat and decision. As a repeater (ryb_comms.json) the library passes
 * every heartbeat packet on to decision by itself; this callback only lets sound look at it. */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "ryb/ryb.h"

/* Called by ryb_update() for each heartbeat packet. Keep it short: store, don't compute. */
void snd_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)
{
    printf("heartbeat: bpm %u, trend %d\n", (unsigned) bpm, (int) trend);
}

/* DUMMY: stands in for the real loudness measurement, 0..100 %. */
static uint8_t read_volume(void)
{
    static int volume = 30;
    volume = volume >= 90 ? 30 : volume + 10;
    return (uint8_t) volume;
}

int main(void)
{
    if (ryb_begin() != RYB_OK) {
        return 1;
    }

    int previous = -1;
    unsigned loops = 0;
    for (;;) {
        ryb_update(); /* runs snd_receive_heartbeat() for whatever arrived */

        if (loops++ % 100u == 0u) {
            const uint8_t volume = read_volume();
            ryb_send_appended(volume, ryb_trend(previous, volume));
            printf("sent volume %u\n", (unsigned) volume);
            previous = volume;
        }

        usleep(10000);
    }
}
