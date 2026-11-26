/* Purpose: push real frames through the RYB Comms bit loop, tx wired to rx in the sim backend.
 * Owning module: management (mgmt/). Built twice: as decision (commands), heartbeat (appended). */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_gpio.h"

#define MAX_PACKETS 16
#define TIMEOUT_MS  5000

typedef struct {
    uint8_t a;
    uint8_t b;
} pair_t;

/* Picked for the awkward cases. Command 21/10 has parity 0, so the copy queued right behind it
 * starts with no falling edge. Appended 0x00/increasing from address 0 is an all-zero frame, so
 * two in a row hold the line low for two whole packets. */
#if RYB_CFG_IS_COMMAND_SENDER
static const pair_t sent[] = {{21u, 10u}, {21u, 10u}, {31u, 31u}, {0u, 0u}, {0u, 0u}, {1u, 30u}};
#else
static const pair_t sent[] = {{0xA5u, RYB_TREND_INCREASING}, {0x00u, RYB_TREND_INCREASING},
                              {0x00u, RYB_TREND_INCREASING}, {0xFFu, RYB_TREND_DECREASING},
                              {0x00u, RYB_TREND_NA},         {0x5Au, RYB_TREND_NA}};
#endif

#define SENT_COUNT ((int) (sizeof sent / sizeof sent[0]))

static pair_t got[MAX_PACKETS];
static int got_count;

static void record(uint8_t a, uint8_t b)
{
    if (got_count < MAX_PACKETS) {
        got[got_count].a = a;
        got[got_count].b = b;
    }
    got_count++;
}

#if RYB_CFG_IS_COMMAND_SENDER
void dec_receive_command(uint8_t first, uint8_t second)
{
    record(first, second);
}
#else
void hb_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)
{
    record(bpm, (uint8_t) trend);
}
#endif

static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static ryb_status_t send_pair(const pair_t* p)
{
#if RYB_CFG_IS_COMMAND_SENDER
    return ryb_send_command(p->a, p->b);
#else
    return ryb_send_appended(p->a, (ryb_trend_t) p->b);
#endif
}

int main(void)
{
    static const struct {
        uint8_t bit;
        uint8_t accept_mask;
    } links[] = RYB_CFG_RX_LINKS;
    ryb_comms_stats_t stats;
    int failures = 0;

    ryb_gpio_sim_connect((uint8_t) RYB_CFG_TX_BIT, links[0].bit);
    if (ryb_comms_init() != RYB_OK) {
        puts("FAIL: ryb_comms_init");
        return 1;
    }

    /* The receiver starts in resync and ignores edges until the line has idled for a packet
     * time, so give it two before sending. */
    sleep_ms(2L * 14L * (long) RYB_CFG_BIT_PERIOD_US / 1000L + 1L);

    for (int i = 0; i < SENT_COUNT; i++) {
        if (send_pair(&sent[i]) != RYB_OK) {
            printf("FAIL: send %d refused\n", i);
            failures++;
        }
    }

    for (int waited = 0; got_count < SENT_COUNT && waited < TIMEOUT_MS; waited++) {
        ryb_comms_poll();
        sleep_ms(1L);
    }
    /* The last packet is handed up half a bit before its sender finishes the parity slot. */
    sleep_ms(2L * (long) RYB_CFG_BIT_PERIOD_US / 1000L + 1L);
    ryb_comms_get_stats(&stats);
    ryb_comms_shutdown();

    if (got_count != SENT_COUNT) {
        printf("FAIL: received %d of %d packets\n", got_count, SENT_COUNT);
        failures++;
    }
    for (int i = 0; i < SENT_COUNT && i < got_count && i < MAX_PACKETS; i++) {
        if (got[i].a != sent[i].a || got[i].b != sent[i].b) {
            printf("FAIL: packet %d: sent %u/%u, received %u/%u\n", i, (unsigned) sent[i].a,
                   (unsigned) sent[i].b, (unsigned) got[i].a, (unsigned) got[i].b);
            failures++;
        }
    }
    if (stats.rx_parity_errors != 0u || stats.rx_address_errors != 0u ||
        stats.rx_trend_errors != 0u || stats.tx_packets != (uint32_t) SENT_COUNT) {
        printf("FAIL: stats rx %u, parity %u, address %u, trend %u, tx %u\n",
               (unsigned) stats.rx_packets, (unsigned) stats.rx_parity_errors,
               (unsigned) stats.rx_address_errors, (unsigned) stats.rx_trend_errors,
               (unsigned) stats.tx_packets);
        failures++;
    }

    if (failures != 0) {
        return 1;
    }
    printf("%s loopback: %d packets back to back, all received intact\n", RYB_CFG_MODULE_NAME,
           SENT_COUNT);
    return 0;
}
