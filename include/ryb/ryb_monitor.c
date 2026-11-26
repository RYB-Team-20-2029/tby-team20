/* Purpose: RYB Comms on its own for one module: shows what arrives, sends test values. For board
 * bring-up and chain tests before the module's main.c exists. Owning module: management (mgmt/).
 *
 *   <module>_monitor [-p send_period_ms] [-r | -s] [-t seconds] [-q] [-n]
 *     -p  how often to send a test packet, 0 for never (default: ryb_comms.json "monitor", 500)
 *     -r  random test values; -s values sweeping up and down (default: "monitor", sweep)
 *     -t  stop after this long, 0 for never (default 0)
 *     -q  no line per received packet
 *     -n  no LCD
 */

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_lcd.h"

static volatile sig_atomic_t stop;
static bool quiet;

/* Only a module that receives something needs these. */
#if RYB_CFG_ACCEPT_MASK != 0u
static const char* const sender_names[] = RYB_CFG_SENDER_NAMES;
static const char* const value_names[] = RYB_CFG_VALUE_NAMES;

static const char* trend_name(uint8_t t)
{
    switch (t) {
        case RYB_TREND_INCREASING:
            return "up";
        case RYB_TREND_DECREASING:
            return "down";
        default:
            return "n/a";
    }
}

static void report(uint8_t address, uint8_t a, uint8_t b)
{
    if (quiet) {
        return;
    }
    if (address == RYB_CFG_COMMAND_ADDRESS) {
        printf("rx %-9s first %u second %u\n", sender_names[address], (unsigned) a, (unsigned) b);
    } else {
        printf("rx %-9s %s %u trend %s\n", sender_names[address], value_names[address],
               (unsigned) a, trend_name(b));
    }
}
#endif

/* Whatever callbacks ryb_comms.json gives this module. The LCD reads the last packet per sender
 * from the library, so these only print. */
#define APPENDED_CALLBACK(fn, address)                                                            \
    void fn(uint8_t value, ryb_trend_t trend)                                                     \
    {                                                                                             \
        report(address, value, (uint8_t) trend);                                                  \
    }
#define COMMAND_CALLBACK(fn, address)                                                             \
    void fn(uint8_t first, uint8_t second)                                                        \
    {                                                                                             \
        report(address, first, second);                                                           \
    }

#ifdef RYB_CFG_CALLBACK_0
#if RYB_CFG_COMMAND_ADDRESS == 0u
COMMAND_CALLBACK(RYB_CFG_CALLBACK_0, 0u)
#else
APPENDED_CALLBACK(RYB_CFG_CALLBACK_0, 0u)
#endif
#endif
#ifdef RYB_CFG_CALLBACK_1
#if RYB_CFG_COMMAND_ADDRESS == 1u
COMMAND_CALLBACK(RYB_CFG_CALLBACK_1, 1u)
#else
APPENDED_CALLBACK(RYB_CFG_CALLBACK_1, 1u)
#endif
#endif
#ifdef RYB_CFG_CALLBACK_2
#if RYB_CFG_COMMAND_ADDRESS == 2u
COMMAND_CALLBACK(RYB_CFG_CALLBACK_2, 2u)
#else
APPENDED_CALLBACK(RYB_CFG_CALLBACK_2, 2u)
#endif
#endif
#ifdef RYB_CFG_CALLBACK_3
#if RYB_CFG_COMMAND_ADDRESS == 3u
COMMAND_CALLBACK(RYB_CFG_CALLBACK_3, 3u)
#else
APPENDED_CALLBACK(RYB_CFG_CALLBACK_3, 3u)
#endif
#endif

static void on_signal(int sig)
{
    (void) sig;
    stop = 1;
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) ts.tv_nsec / 1000000u;
}

static uint32_t rng;

/* A value in lo..hi. Not for anything but test traffic. */
static int random_in(int lo, int hi)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return lo + (int) (rng % (uint32_t) (hi - lo + 1));
}

/* The next test value, in a range that suits the module, so a display at the far end shows
 * values and trends that move: random, or sweeping up and down. */
static void send_test_value(bool use_random)
{
    static int value = -1;
#if RYB_CFG_IS_COMMAND_SENDER
    uint16_t first;
    uint16_t second;
    if (use_random) {
        first = (uint16_t) random_in(1, 5);
        second = (uint16_t) random_in(1, 5);
    } else {
        /* Regions 1..5 for both segments, walking in opposite directions. */
        value = (value + 1) % 5;
        first = (uint16_t) (value + 1);
        second = (uint16_t) (5 - value);
    }
    if (!quiet) {
        printf("tx command first %u second %u\n", (unsigned) first, (unsigned) second);
    }
    const ryb_status_t rc = ryb_send_command(first, second);
#else
    static int step = 0;
    int lo = 0;
    int hi = 255;
    int inc = 15;
    if (RYB_CFG_ADDRESS == RYB_ADDR_HEARTBEAT) {
        lo = 60;
        hi = 180;
        inc = 3;
    } else if (RYB_CFG_ADDRESS == RYB_ADDR_SOUND) {
        lo = 0;
        hi = 100;
        inc = 5;
    }
    const int previous = value;
    ryb_trend_t trend = RYB_TREND_NA;
    if (use_random) {
        value = random_in(lo, hi);
        if (previous >= 0 && value != previous) {
            trend = value > previous ? RYB_TREND_INCREASING : RYB_TREND_DECREASING;
        }
    } else if (value < 0) {
        value = lo;
        step = inc;
    } else {
        value += step;
        if (value >= hi || value <= lo) {
            value = value >= hi ? hi : lo;
            step = -step;
        }
        trend = step > 0 ? RYB_TREND_INCREASING : RYB_TREND_DECREASING;
    }
    if (!quiet) {
        printf("tx %d\n", value);
    }
    const ryb_status_t rc = ryb_send_appended((uint8_t) value, trend);
#endif
    if (rc != RYB_OK && !quiet) {
        printf("send failed: %d\n", (int) rc);
    }
}

int main(int argc, char** argv)
{
    long period_ms = (long) RYB_CFG_MONITOR_PERIOD_MS;
    bool use_random = RYB_CFG_MONITOR_RANDOM != 0;
    long seconds = 0;
    bool lcd = true;
    int opt;
    while ((opt = getopt(argc, argv, "p:rst:qn")) != -1) {
        switch (opt) {
            case 'p':
                period_ms = atol(optarg);
                break;
            case 'r':
                use_random = true;
                break;
            case 's':
                use_random = false;
                break;
            case 't':
                seconds = atol(optarg);
                break;
            case 'q':
                quiet = true;
                break;
            case 'n':
                lcd = false;
                break;
            default:
                fprintf(stderr, "usage: %s [-p send_period_ms] [-r|-s] [-t seconds] [-q] [-n]\n",
                        argv[0]);
                return 2;
        }
    }

    /* Own handlers, so the library leaves the signals to us and we shut it down cleanly. */
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (ryb_comms_init() != RYB_OK) {
        fprintf(stderr, "ryb_comms_init failed (run as root on the PYNQ)\n");
        return 1;
    }
    if (lcd && ryb_lcd_init() != RYB_OK) {
        fprintf(stderr, "no LCD, carrying on without it\n");
        lcd = false;
    }
    rng = (uint32_t) time(NULL) ^ ((uint32_t) getpid() << 16) ^ 0x9E3779B9u;
    const bool sending = RYB_CFG_TX_BIT >= 0 && period_ms > 0;
    printf("%s monitor: address %u, tx %s, rx %s, %u us bits, %s\n", RYB_CFG_MODULE_NAME,
           (unsigned) RYB_CFG_ADDRESS, RYB_CFG_TX_NAME, RYB_CFG_RX_NAMES,
           (unsigned) RYB_CFG_BIT_PERIOD_US, lcd ? "LCD on" : "no LCD");
    if (sending) {
        printf("sending %s test values every %ld ms\n", use_random ? "random" : "sweeping",
               period_ms);
    }

    const uint64_t start = now_ms();
    uint64_t next_send = start;
    uint64_t next_screen = start;
    uint64_t next_status = start + 1000u;
    while (!stop && (seconds == 0 || now_ms() - start < (uint64_t) seconds * 1000u)) {
        ryb_comms_poll();
        const uint64_t now = now_ms();
        if (sending && now >= next_send) {
            send_test_value(use_random);
            next_send += (uint64_t) period_ms;
        }
        if (lcd && now >= next_screen) {
            ryb_lcd_show_comms();
            next_screen = now + 200u;
        }
        if (now >= next_status) {
            ryb_comms_stats_t s;
            ryb_comms_get_stats(&s);
            printf("status: rx %lu, errors %lu/%lu/%lu (parity/address/trend), tx %lu, "
                   "dropped %lu/%lu (rx/tx)\n",
                   (unsigned long) s.rx_packets, (unsigned long) s.rx_parity_errors,
                   (unsigned long) s.rx_address_errors, (unsigned long) s.rx_trend_errors,
                   (unsigned long) s.tx_packets, (unsigned long) s.rx_overflows,
                   (unsigned long) s.tx_overflows);
            next_status += 1000u;
        }
        const struct timespec tick = {0, 10000000L};
        nanosleep(&tick, NULL);
    }

    ryb_comms_shutdown();
    if (lcd) {
        ryb_lcd_close();
    }
    return 0;
}
