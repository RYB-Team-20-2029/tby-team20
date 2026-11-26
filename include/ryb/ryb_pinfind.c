/* Purpose: map which header pins connect between boards through the connector boards and
 * backbone, without a scope. Owning module: management (mgmt/).
 *
 *   ryb_pinfind talk   [seconds]   beacon on every free pin in turn (default: until stopped)
 *   ryb_pinfind listen [seconds]   watch every free pin, then report (default 20)
 *
 * The talker sends RYB packets on one pin at a time, each carrying that pin's number. A listener
 * decodes all its pins at once, so one run tells it which of its pins is wired to which of the
 * talker's. Run it with every board's ryb-comms stopped (tools/pinfind.sh does that), and only
 * one talker at a time: two would drive the same conductors against each other.
 */

#define _GNU_SOURCE

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_gpio.h"

/* Millisecond bits: slow enough for any wiring and for a talker that is not real-time. */
#define BIT_NS          1000000ull
#define BEACONS_PER_PIN 3
/* Beacons go out as motor packets: the motor never sends, so real traffic cannot pass for one. */
#define BEACON_ADDRESS RYB_ADDR_MOTOR
#define MAX_PINS       64

typedef struct {
    const char* name;
    uint8_t bit;
} pin_t;

typedef enum {
    L_WAIT_IDLE, /* line must stay high for a few bits before an edge counts */
    L_ARMED,     /* waiting for the start bit's falling edge */
    L_FRAME      /* sampling the start bit and 13 frame bits at their midpoints */
} listen_state_t;

typedef struct {
    listen_state_t state;
    uint8_t slot;
    uint8_t last;
    uint16_t shift;
    uint64_t t_high;
    uint64_t t_next;
    uint32_t edges;
    uint32_t bad;
} listener_t;

static const pin_t all_pins[] = RYB_CFG_PINS;
static pin_t pins[MAX_PINS];
static int pin_count;
static listener_t listeners[MAX_PINS];
static uint32_t heard[MAX_PINS][64]; /* [this board's pin][talker's GPIO bit] */
static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
    (void) sig;
    stop = 1;
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static void hold_until(uint64_t t)
{
    while (now_ns() < t) {
    }
}

static bool lcd_pin(uint8_t bit)
{
    return RYB_CFG_HAS_LCD &&
           (bit == RYB_CFG_LCD_BL || bit == RYB_CFG_LCD_RST || bit == RYB_CFG_LCD_DC ||
            bit == RYB_CFG_LCD_CS || bit == RYB_CFG_LCD_SCLK || bit == RYB_CFG_LCD_MOSI);
}

static const char* pin_name(uint8_t bit)
{
    for (size_t i = 0u; i < sizeof all_pins / sizeof all_pins[0]; i++) {
        if (all_pins[i].bit == bit) {
            return all_pins[i].name;
        }
    }
    return "?";
}

/* Every pin in ryb_comms.json except the display's. */
static void pick_pins(void)
{
    for (size_t i = 0u; i < sizeof all_pins / sizeof all_pins[0] && pin_count < MAX_PINS; i++) {
        if (!lcd_pin(all_pins[i].bit)) {
            pins[pin_count++] = all_pins[i];
        }
    }
}

static void beacon(uint8_t bit)
{
    uint16_t frame = 0u;
    ryb_pack_appended(BEACON_ADDRESS, bit, RYB_TREND_NA, &frame);

    ryb_gpio_write(bit, 1u);
    ryb_gpio_set_output(bit);
    uint64_t t = now_ns() + 4u * BIT_NS; /* idle first, so listeners arm */
    hold_until(t);
    for (int n = 0; n < BEACONS_PER_PIN && !stop; n++) {
        ryb_gpio_write(bit, 0u);
        for (int i = 12; i >= -1; i--) {
            t += BIT_NS;
            hold_until(t);
            ryb_gpio_write(bit, i >= 0 ? (uint8_t) ((frame >> i) & 1u) : 1u);
        }
        t += 4u * BIT_NS;
        hold_until(t);
    }
    ryb_gpio_set_input(bit);
}

static void talk_all(long seconds)
{
    const uint64_t end = seconds > 0 ? now_ns() + (uint64_t) seconds * 1000000000ull : 0u;
    printf("Talking: a beacon on each of %d pins in turn, %.1f s a round. Stop with Ctrl-C.\n",
           pin_count, pin_count * (4 + BEACONS_PER_PIN * 18) / 1000.0);
    for (unsigned round = 1u; !stop && (end == 0u || now_ns() < end); round++) {
        printf("round %u\n", round);
        fflush(stdout);
        for (int i = 0; i < pin_count && !stop; i++) {
            beacon(pins[i].bit);
        }
    }
}

static void listen_step(int i, uint8_t level, uint64_t now)
{
    listener_t* l = &listeners[i];
    if (level != l->last) {
        l->edges++;
        l->last = level;
    }
    switch (l->state) {
        case L_WAIT_IDLE:
            if (level == 0u) {
                l->t_high = 0u;
            } else if (l->t_high == 0u) {
                l->t_high = now;
            } else if (now - l->t_high >= 3u * BIT_NS) {
                l->state = L_ARMED;
            }
            break;
        case L_ARMED:
            if (level == 0u) {
                l->slot = 0u;
                l->shift = 0u;
                l->t_next = now + BIT_NS / 2u;
                l->state = L_FRAME;
            }
            break;
        case L_FRAME:
            if (now < l->t_next) {
                break;
            }
            if (l->slot == 0u && level != 0u) {
                l->t_high = 0u;
                l->state = L_WAIT_IDLE;
                break;
            }
            if (l->slot > 0u) {
                l->shift = (uint16_t) ((l->shift << 1) | level);
            }
            l->slot++;
            l->t_next += BIT_NS;
            if (l->slot == RYB_FRAME_BITS + 1u) {
                ryb_packet_t pkt;
                if (ryb_frame_decode(l->shift, 1u << BEACON_ADDRESS, RYB_ADDR_DECISION, &pkt) ==
                        RYB_OK &&
                    pkt.u.appended.trend == RYB_TREND_NA && pkt.u.appended.data < 64u) {
                    heard[i][pkt.u.appended.data]++;
                } else {
                    l->bad++;
                }
                l->t_high = 0u;
                l->state = L_WAIT_IDLE;
            }
            break;
    }
}

static void listen_all(long seconds)
{
    for (int i = 0; i < pin_count; i++) {
        ryb_gpio_set_input(pins[i].bit);
        listeners[i].last = ryb_gpio_read(pins[i].bit);
    }
    printf("Listening on %d pins for %ld s; start the talker on the other board now.\n",
           pin_count, seconds);
    fflush(stdout);
    const uint64_t end = now_ns() + (uint64_t) seconds * 1000000000ull;
    for (uint64_t now = now_ns(); now < end && !stop; now = now_ns()) {
        for (int i = 0; i < pin_count; i++) {
            listen_step(i, ryb_gpio_read(pins[i].bit), now);
        }
    }

    int found = 0;
    printf("\nPins on this board that hear the talker:\n");
    for (int i = 0; i < pin_count; i++) {
        for (int b = 0; b < 64; b++) {
            if (heard[i][b] != 0u) {
                printf("  %-7s <- talker's %-7s (%lu beacons)\n", pins[i].name, pin_name((uint8_t) b),
                       (unsigned long) heard[i][b]);
                found++;
            }
        }
    }
    if (found == 0) {
        printf("  none. Check that the talker is running and that both boards share GND.\n");
    }

    bool noisy = false;
    for (int i = 0; i < pin_count; i++) {
        bool any = false;
        for (int b = 0; b < 64; b++) {
            any = any || heard[i][b] != 0u;
        }
        if (!any && listeners[i].edges > 10u) {
            if (!noisy) {
                printf("\nPins with activity but no valid beacon (floating, noise or other traffic):\n");
                noisy = true;
            }
            printf("  %-7s %lu edges\n", pins[i].name, (unsigned long) listeners[i].edges);
        }
    }
    if (found > 0) {
        printf("\nFor ryb_comms.json, a link from the talking board's module to this one's uses:\n"
               "  talker:  \"tx\": \"<talker's pin>\"\n"
               "  here:    \"rx\": [ { \"pin\": \"<this board's pin>\", \"from\": [ \"<talker's "
               "module>\" ] } ]\n");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || (strcmp(argv[1], "talk") != 0 && strcmp(argv[1], "listen") != 0)) {
        fprintf(stderr, "usage: %s talk|listen [seconds]\n", argv[0]);
        return 2;
    }
    const bool talking = strcmp(argv[1], "talk") == 0;
    const long seconds = argc > 2 ? atol(argv[2]) : (talking ? 0 : 20);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    if (ryb_gpio_open(RYB_CFG_GPIO_BASE) != RYB_OK) {
        fprintf(stderr, "cannot map the GPIO (run as root on the PYNQ)\n");
        return 1;
    }
    pick_pins();
    if (talking) {
        talk_all(seconds);
    } else {
        listen_all(seconds);
    }
    ryb_gpio_close();
    return 0;
}
