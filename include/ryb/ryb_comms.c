/* Purpose: RYB Comms link layer: the bit-timed TX/RX loop, repeater forwarding, callback dispatch.
 * Owning module: management (mgmt/). */

#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_gpio.h"

#define NS_PER_US   1000ull
#define BIT_NS      ((uint64_t) RYB_CFG_BIT_PERIOD_US * NS_PER_US)
#define HALF_BIT_NS (BIT_NS / 2u)

/* On the wire a packet is its start bit plus the 13 frame bits. */
#define SLOT_BITS 14u
#define PACKET_NS (BIT_NS * SLOT_BITS)

/* Longest the loop sleeps while a link waits for an edge. A module with nothing to listen to
 * only has to notice queued sends now and then. */
#define IDLE_SLEEP_NS                                                                             \
    (RYB_CFG_RX_LINK_COUNT > 0 ? (uint64_t) RYB_CFG_IDLE_SLEEP_US * NS_PER_US : 1000000ull)
#define MAX_SLEEP_NS 1000000ull /* keeps ryb_comms_shutdown() prompt */

/* Inside a packet the loop sleeps too, waking this long before the next bit deadline. On the
 * PYNQ-Z2 nanosleep() overshot by 26 us typically and 74 us at worst, so a late wake lands well
 * inside half a bit. Without these sleeps a sustained burst holds the SCHED_FIFO thread at 100 %
 * CPU until the kernel's RT throttling freezes it for about 50 ms, mid-packet. */
#define WAKE_GUARD_NS 60000ull

/* The guideline recovers alignment from an idle line, so a receiver that hits a bad frame hears
 * nothing more until the line goes quiet. A sender idles after this many back-to-back packets,
 * so a saturated link still gives it that chance. */
#define MAX_BURST 4u
#define GAP_NS    (PACKET_NS + 2u * BIT_NS)

/* The line picks up noise spikes a few microseconds wide. Each bit is read three times, an
 * eighth of a bit apart around its middle, and the majority wins, so one spike can flip one read
 * but not the bit. Outside a bit, a level only counts once it has held for SPIKE_NS. */
#define VOTES          3u
#define VOTE_SPREAD_NS (BIT_NS / 8u)
#define SPIKE_NS       10000ull

#define HAS_TX     (RYB_CFG_TX_BIT >= 0)
#define TX_BIT     ((uint8_t) RYB_CFG_TX_BIT)
#define RX_SLOTS   (RYB_CFG_RX_LINK_COUNT > 0 ? RYB_CFG_RX_LINK_COUNT : 1)
#define ACCEPT_ANY 0x0Fu

#define QUEUE_SLOTS 128u /* power of two, so the free-running indices wrap cleanly */

#define COUNT(field) atomic_fetch_add_explicit(&counters.field, 1u, memory_order_relaxed)

/* Single producer, single consumer: only the producer writes head, only the consumer tail. */
typedef struct {
    uint16_t slot[QUEUE_SLOTS];
    unsigned cap;
    atomic_uint head;
    atomic_uint tail;
} frame_queue_t;

typedef enum {
    RX_RESYNC, /* waiting for one packet time of idle line before trusting an edge */
    RX_IDLE,   /* line high, armed for the falling edge of a start bit */
    RX_START,  /* edge seen, voting on the start bit */
    RX_DATA,   /* voting on the 13 frame bits, MSB first */
    RX_TAIL    /* frame done; a line still low here may be a back-to-back start bit */
} rx_state_t;

typedef struct {
    uint8_t bit;
    uint8_t accept_mask;
} rx_config_t;

typedef struct {
    rx_config_t cfg;
    rx_state_t state;
    uint8_t slot;     /* bit slot being read: 0 is the start bit, 1..13 the frame bits */
    uint8_t sub;      /* reads taken of the current slot */
    uint8_t votes;    /* reads of the current slot that saw a 1 */
    uint16_t shift;
    uint64_t t_start; /* falling edge of the current start bit */
    uint64_t t_next;  /* next read */
    uint64_t t_high;  /* RX_RESYNC: when the line was seen rising; 0 while it is low */
    uint64_t t_seen;  /* RX_IDLE: when the line was last seen high */
    uint64_t t_other; /* RX_RESYNC, RX_TAIL: since when the line has read the other level */
} rx_link_t;

typedef enum {
    TX_IDLE,   /* line released, nothing queued */
    TX_ACTIVE, /* inside a packet: start bit, then the frame bits MSB first */
    TX_GAP     /* line held idle after a full burst */
} tx_state_t;

typedef struct {
    tx_state_t state;
    uint16_t frame;
    int8_t index;        /* next frame bit to drive; -1 once parity is on the line */
    uint8_t burst;       /* packets since the line last idled for a packet time */
    uint64_t t_next;     /* end of the current bit slot, or of the gap */
    uint64_t t_released; /* when the line was last released */
} tx_link_t;

static const rx_config_t rx_config[] = RYB_CFG_RX_LINKS;
static rx_link_t rx_links[RX_SLOTS];
static tx_link_t tx_link;

/* Deep on receive, so a main loop busy on an LCD refresh loses nothing for a second or so.
 * Shallow on transmit, so RYB_ERR_BUSY reports a saturated line before queued data goes stale. */
static frame_queue_t rx_queue = {.cap = 128u};  /* bit loop -> ryb_comms_poll()    */
static frame_queue_t tx_queue = {.cap = 32u};   /* ryb_send_*() -> bit loop        */
static frame_queue_t fwd_queue = {.cap = 32u};  /* bit loop -> bit loop, repeaters */

static struct {
    atomic_uint rx_packets;
    atomic_uint rx_parity_errors;
    atomic_uint rx_address_errors;
    atomic_uint rx_trend_errors;
    atomic_uint rx_overflows;
    atomic_uint tx_packets;
    atomic_uint tx_overflows;
    atomic_uint forwarded;
} counters;

/* Last packet per address: received ones, and this module's own sends under its own address.
 * Only touched from the thread that polls and sends, so plain fields are enough. */
static ryb_comms_sender_t senders[RYB_ADDR_MAX + 1u];

static pthread_t loop_thread;
static atomic_bool running;
/* Read by the signal handler, hence sig_atomic_t. */
static volatile sig_atomic_t initialised;
static bool exit_hook_registered;

static bool queue_push(frame_queue_t* q, uint16_t frame)
{
    const unsigned head = atomic_load_explicit(&q->head, memory_order_relaxed);
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (head - tail >= q->cap) {
        return false;
    }
    q->slot[head % QUEUE_SLOTS] = frame;
    atomic_store_explicit(&q->head, head + 1u, memory_order_release);
    return true;
}

static bool queue_pop(frame_queue_t* q, uint16_t* frame)
{
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (head == tail) {
        return false;
    }
    *frame = q->slot[tail % QUEUE_SLOTS];
    atomic_store_explicit(&q->tail, tail + 1u, memory_order_release);
    return true;
}

static void queue_reset(frame_queue_t* q)
{
    atomic_store(&q->head, 0u);
    atomic_store(&q->tail, 0u);
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static void sleep_ns(uint64_t ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t) (ns / 1000000000ull);
    ts.tv_nsec = (long) (ns % 1000000000ull);
    nanosleep(&ts, NULL);
}

static void drive(uint8_t level)
{
    if (RYB_CFG_OPEN_DRAIN) {
        /* The data bit was set to 0 at init and stays there: a 0 makes the pin an output that
         * pulls low, a 1 releases it to the pull-up. The pin never drives the lane high. */
        if (level != 0u) {
            ryb_gpio_set_input(TX_BIT);
        } else {
            ryb_gpio_set_output(TX_BIT);
        }
    } else {
        ryb_gpio_write(TX_BIT, level);
    }
}

static void release_tx(void)
{
    if (HAS_TX) {
        /* High impedance in both drive modes: an idle open-drain lane belongs to its pull-up,
         * and a released pin cannot fight another board's driver. */
        ryb_gpio_set_input(TX_BIT);
    }
}

/* Validate a clocked-in frame and queue it. Returns false when the frame suggests the receiver
 * has lost alignment, so it should wait for an idle line before trusting another edge. */
static bool accept_frame(uint16_t frame, uint8_t accept_mask)
{
    ryb_packet_t pkt;

    switch (ryb_frame_decode(frame, accept_mask, RYB_CFG_COMMAND_ADDRESS, &pkt)) {
        case RYB_OK:
            break;
        case RYB_ERR_PARITY:
            COUNT(rx_parity_errors);
            return false;
        case RYB_ERR_ADDRESS:
            if (ryb_frame_sender(frame) == RYB_CFG_ADDRESS) {
                /* Our own packet heard back on a shared lane: expected, and not forwarded. */
                return true;
            }
            COUNT(rx_address_errors);
            return false;
        case RYB_ERR_TREND:
            COUNT(rx_trend_errors);
            return false;
        default:
            return false;
    }

    if (queue_push(&rx_queue, frame)) {
        COUNT(rx_packets);
    } else {
        COUNT(rx_overflows);
    }
    if (RYB_CFG_REPEATER) {
        if (queue_push(&fwd_queue, frame)) {
            COUNT(forwarded);
        } else {
            COUNT(tx_overflows);
        }
    }
    return true;
}

/* When read `sub` of the current slot is due. */
static uint64_t rx_read_time(const rx_link_t* rx, uint8_t sub)
{
    return rx->t_start + (uint64_t) rx->slot * BIT_NS + HALF_BIT_NS - VOTE_SPREAD_NS +
           (uint64_t) sub * VOTE_SPREAD_NS;
}

/* Start reading slot `slot` of the packet whose start bit fell at rx->t_start. */
static void rx_begin_slot(rx_link_t* rx, uint8_t slot)
{
    rx->slot = slot;
    rx->sub = 0u;
    rx->votes = 0u;
    rx->t_next = rx_read_time(rx, 0u);
}

/* Take a read if one is due. Returns true once the slot's last read is in, with the majority
 * in *bit. */
static bool rx_vote(rx_link_t* rx, uint64_t now, uint8_t level, uint8_t* bit)
{
    if (now < rx->t_next) {
        return false;
    }
    rx->votes = (uint8_t) (rx->votes + level);
    rx->sub++;
    if (rx->sub < VOTES) {
        rx->t_next = rx_read_time(rx, rx->sub);
        return false;
    }
    *bit = (uint8_t) (rx->votes * 2u > VOTES);
    return true;
}

static void rx_resync(rx_link_t* rx)
{
    rx->t_high = 0u;
    rx->t_other = 0u;
    rx->state = RX_RESYNC;
}

/* Advance one receive link. Returns the latest time the loop may look at it next: just before
 * its next read inside a packet, one idle sleep away while it watches for an edge. */
static uint64_t rx_step(rx_link_t* rx, uint64_t now)
{
    const uint8_t level = ryb_gpio_read(rx->cfg.bit);
    uint8_t bit = 0u;

    switch (rx->state) {
        case RX_RESYNC:
            /* The guideline's recovery rule: a line high for longer than a packet takes is idle,
             * so the next falling edge is a genuine start bit. Only a low that lasts restarts
             * the count; a spike does not. */
            if (level != 0u) {
                rx->t_other = 0u;
                if (rx->t_high == 0u) {
                    rx->t_high = now;
                } else if (now - rx->t_high >= PACKET_NS) {
                    rx->t_seen = now;
                    rx->state = RX_IDLE;
                }
            } else if (rx->t_other == 0u) {
                rx->t_other = now;
            } else if (now - rx->t_other >= SPIKE_NS) {
                rx->t_high = 0u;
            }
            break;

        case RX_IDLE:
            if (level != 0u) {
                rx->t_seen = now;
                rx->t_other = 0u;
            } else if (rx->t_other == 0u) {
                rx->t_other = now;
            } else if (now - rx->t_other >= SPIKE_NS) {
                /* A low that held: a start bit, not a spike. A spike taken for the edge would
                 * put the whole packet off its grid, where a shifted frame passes parity half
                 * the time. The edge fell between the last high read and the first low one;
                 * taking the middle keeps a loop that slept through it off by at most half that
                 * sleep, either way, instead of always late. */
                rx->t_start = rx->t_seen + (rx->t_other - rx->t_seen) / 2u;
                rx_begin_slot(rx, 0u);
                rx->state = RX_START;
            }
            break;

        case RX_START:
            if (rx_vote(rx, now, level, &bit)) {
                if (bit == 0u) {
                    rx->shift = 0u;
                    rx_begin_slot(rx, 1u);
                    rx->state = RX_DATA;
                } else {
                    rx->t_seen = now;
                    rx->t_other = 0u;
                    rx->state = RX_IDLE;
                }
            }
            break;

        case RX_DATA:
            if (rx_vote(rx, now, level, &bit)) {
                rx->shift = (uint16_t) ((rx->shift << 1) | bit);
                if (rx->slot < RYB_FRAME_BITS) {
                    rx_begin_slot(rx, (uint8_t) (rx->slot + 1u));
                } else if (accept_frame(rx->shift, rx->cfg.accept_mask)) {
                    rx->t_other = 0u;
                    rx->t_next = rx->t_start + PACKET_NS + HALF_BIT_NS - VOTE_SPREAD_NS;
                    rx->state = RX_TAIL;
                } else {
                    rx_resync(rx);
                }
            }
            break;

        case RX_TAIL:
            if (level != 0u) {
                /* Released, unless it is a spike: the line has to stay high for a moment. */
                if (rx->t_other == 0u) {
                    rx->t_other = now;
                } else if (now - rx->t_other >= SPIKE_NS) {
                    rx->t_seen = now;
                    rx->t_other = 0u;
                    rx->state = RX_IDLE;
                }
            } else {
                rx->t_other = 0u;
                if (now >= rx->t_next) {
                    /* Still low where the next slot's first read falls: the start bit of a
                     * packet sent back to back. After a parity bit of 0 it has no falling edge,
                     * so keep the bit grid and let the start bit vote confirm it. */
                    rx->t_start += PACKET_NS;
                    rx_begin_slot(rx, 0u);
                    rx->state = RX_START;
                }
            }
            break;
    }

    if (rx->state == RX_START || rx->state == RX_DATA) {
        return rx->t_next - WAKE_GUARD_NS;
    }
    return now + IDLE_SLEEP_NS;
}

static bool next_tx_frame(uint16_t* frame)
{
    /* Forwards first, so a repeater's own packet goes out behind what it passes along. */
    return queue_pop(&fwd_queue, frame) || queue_pop(&tx_queue, frame);
}

static void tx_begin(tx_link_t* tx, uint16_t frame, uint64_t t_slot)
{
    drive(0u); /* start bit */
    tx->frame = frame;
    tx->index = (int8_t) (RYB_FRAME_BITS - 1u);
    tx->burst++;
    tx->t_next = t_slot + BIT_NS;
    tx->state = TX_ACTIVE;
}

/* Advance the transmit link. Returns the latest time the loop may look at it next. */
static uint64_t tx_step(tx_link_t* tx, uint64_t now)
{
    uint16_t frame;

    if (tx->state == TX_GAP && now >= tx->t_next) {
        /* Gap over: whatever queued up during it goes straight out. */
        tx->burst = 0u;
        tx->state = TX_IDLE;
    }

    switch (tx->state) {
        case TX_IDLE:
            if (next_tx_frame(&frame)) {
                /* A packet sent soon after the last one still counts towards the same burst. */
                if (now - tx->t_released >= GAP_NS) {
                    tx->burst = 0u;
                }
                tx_begin(tx, frame, now);
            }
            break;

        case TX_ACTIVE:
            if (now >= tx->t_next) {
                if (tx->index >= 0) {
                    drive((uint8_t) ((tx->frame >> tx->index) & 1u));
                    tx->index--;
                    tx->t_next += BIT_NS;
                } else {
                    COUNT(tx_packets);
                    if (tx->burst < MAX_BURST && next_tx_frame(&frame)) {
                        /* Nothing separates back-to-back packets: the next start bit follows
                         * the parity slot directly. */
                        tx_begin(tx, frame, tx->t_next);
                    } else {
                        drive(1u);
                        tx->t_released = tx->t_next;
                        if (tx->burst >= MAX_BURST) {
                            tx->t_next += GAP_NS;
                            tx->state = TX_GAP;
                        } else {
                            tx->state = TX_IDLE;
                        }
                    }
                }
            }
            break;

        case TX_GAP:
            break;
    }

    switch (tx->state) {
        case TX_ACTIVE:
            return tx->t_next - WAKE_GUARD_NS;
        case TX_GAP:
            return tx->t_next;
        default:
            return now + IDLE_SLEEP_NS; /* how soon a newly queued packet is noticed */
    }
}

static void* bit_loop(void* arg)
{
    (void) arg;

    while (atomic_load_explicit(&running, memory_order_relaxed)) {
        const uint64_t now = now_ns();
        uint64_t wake = now + MAX_SLEEP_NS;

        /* Receive before transmit, so a pin looped back to one of our own inputs is sampled
         * before the next bit replaces it. */
        for (int i = 0; i < RYB_CFG_RX_LINK_COUNT; i++) {
            const uint64_t link_wake = rx_step(&rx_links[i], now);
            if (link_wake < wake) {
                wake = link_wake;
            }
        }
        if (HAS_TX) {
            const uint64_t link_wake = tx_step(&tx_link, now);
            if (link_wake < wake) {
                wake = link_wake;
            }
        }

        if (wake > now) {
            sleep_ns(wake - now);
        }
    }
    return NULL;
}

static int start_bit_loop(void)
{
    pthread_attr_t attr;
    int err;

    pthread_attr_init(&attr);
    if (RYB_CFG_THREAD_REALTIME) {
        /* SCHED_FIFO keeps ordinary processes from preempting the loop in the middle of a bit. */
        struct sched_param param;
        memset(&param, 0, sizeof param);
        param.sched_priority = RYB_CFG_THREAD_PRIORITY;
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        pthread_attr_setschedparam(&attr, &param);
    }
    err = pthread_create(&loop_thread, &attr, bit_loop, NULL);
    pthread_attr_destroy(&attr);

    if (err == EPERM && RYB_CFG_THREAD_REALTIME) {
        fprintf(stderr, "ryb_comms: no permission for real-time scheduling, "
                        "the bit loop runs at normal priority\n");
        err = pthread_create(&loop_thread, NULL, bit_loop, NULL);
    }

#if RYB_CFG_THREAD_CPU >= 0
    if (err == 0) {
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(RYB_CFG_THREAD_CPU, &cpus);
        if (pthread_setaffinity_np(loop_thread, sizeof cpus, &cpus) != 0) {
            fprintf(stderr, "ryb_comms: could not pin the bit loop to CPU %d\n",
                    RYB_CFG_THREAD_CPU);
        }
    }
#endif

    return err;
}

static void on_fatal_signal(int sig)
{
    if (initialised && HAS_TX) {
        ryb_gpio_release(TX_BIT);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void hook_signal(int sig)
{
    struct sigaction old;

    /* Only take a signal nobody has claimed; a module with its own handler releases the line by
     * calling ryb_comms_shutdown() from it. */
    if (sigaction(sig, NULL, &old) == 0 && old.sa_handler == SIG_DFL) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = on_fatal_signal;
        sigemptyset(&sa.sa_mask);
        sigaction(sig, &sa, NULL);
    }
}

static void remember(uint16_t frame, const ryb_packet_t* pkt)
{
    ryb_comms_sender_t* s = &senders[pkt->sender];
    s->count++;
    s->t_last_ns = now_ns();
    s->frame = frame;
    s->packet = *pkt;
}

static void deliver(const ryb_packet_t* pkt)
{
    /* RYB_CFG_DELIVER_<n> exists only for the senders ryb_comms.json lists for this module. */
    switch (pkt->sender) {
#ifdef RYB_CFG_DELIVER_0
        case 0u:
            RYB_CFG_DELIVER_0(pkt);
            break;
#endif
#ifdef RYB_CFG_DELIVER_1
        case 1u:
            RYB_CFG_DELIVER_1(pkt);
            break;
#endif
#ifdef RYB_CFG_DELIVER_2
        case 2u:
            RYB_CFG_DELIVER_2(pkt);
            break;
#endif
#ifdef RYB_CFG_DELIVER_3
        case 3u:
            RYB_CFG_DELIVER_3(pkt);
            break;
#endif
        default:
            break;
    }
}

ryb_status_t ryb_comms_init(void)
{
    if (initialised) {
        return RYB_ERR_STATE;
    }

    const ryb_status_t rc = ryb_gpio_open(RYB_CFG_GPIO_BASE);
    if (rc != RYB_OK) {
        return rc;
    }

    if (HAS_TX) {
        /* Release first, so a pin a crashed run left pulled low frees the lane straight away. */
        ryb_gpio_set_input(TX_BIT);
        if (RYB_CFG_OPEN_DRAIN) {
            ryb_gpio_write(TX_BIT, 0u);
        } else {
            ryb_gpio_write(TX_BIT, 1u);
            ryb_gpio_set_output(TX_BIT);
        }
    }
    for (int i = 0; i < RYB_CFG_RX_LINK_COUNT; i++) {
        rx_links[i].cfg = rx_config[i];
        if (!HAS_TX || rx_config[i].bit != TX_BIT) {
            ryb_gpio_set_input(rx_config[i].bit);
        }
        /* Start distrustful: this board may be joining a line that is mid-packet. */
        rx_resync(&rx_links[i]);
    }
    sleep_ns((uint64_t) RYB_CFG_DIRECTION_SETTLE_US * NS_PER_US);

    tx_link.state = TX_IDLE;
    tx_link.burst = 0u;
    tx_link.t_released = 0u;
    queue_reset(&rx_queue);
    queue_reset(&tx_queue);
    queue_reset(&fwd_queue);

    atomic_store(&running, true);
    if (start_bit_loop() != 0) {
        perror("ryb_comms: pthread_create");
        atomic_store(&running, false);
        release_tx();
        ryb_gpio_close();
        return RYB_ERR_STATE;
    }
    initialised = 1;

    if (HAS_TX) {
        hook_signal(SIGINT);
        hook_signal(SIGTERM);
    }
    if (!exit_hook_registered) {
        atexit(ryb_comms_shutdown);
        exit_hook_registered = true;
    }
    return RYB_OK;
}

void ryb_comms_shutdown(void)
{
    if (!initialised) {
        return;
    }
    /* Cleared first, so a signal arriving from here on leaves the GPIO to this function. */
    initialised = 0;
    atomic_store(&running, false);
    pthread_join(loop_thread, NULL);
    release_tx();
    ryb_gpio_close();
}

int ryb_comms_poll(void)
{
    uint16_t frame;
    ryb_packet_t pkt;
    int delivered = 0;

    while (queue_pop(&rx_queue, &frame)) {
        /* Already checked in the bit loop; decoding here keeps the queue to plain frames. */
        if (ryb_frame_decode(frame, ACCEPT_ANY, RYB_CFG_COMMAND_ADDRESS, &pkt) == RYB_OK) {
            remember(frame, &pkt);
            deliver(&pkt);
            delivered++;
        }
    }
    return delivered;
}

static ryb_status_t enqueue(uint16_t frame)
{
    if (!initialised || !HAS_TX) {
        return RYB_ERR_STATE;
    }
    if (!queue_push(&tx_queue, frame)) {
        COUNT(tx_overflows);
        return RYB_ERR_BUSY;
    }
    ryb_packet_t pkt;
    if (ryb_frame_decode(frame, ACCEPT_ANY, RYB_CFG_COMMAND_ADDRESS, &pkt) == RYB_OK) {
        remember(frame, &pkt);
    }
    return RYB_OK;
}

void ryb_comms_get_sender(uint8_t address, ryb_comms_sender_t* out)
{
    if (out != NULL && address <= RYB_ADDR_MAX) {
        *out = senders[address];
    }
}

#if RYB_CFG_IS_COMMAND_SENDER
ryb_status_t ryb_send_command(uint16_t first, uint16_t second)
{
    uint16_t frame = 0u;
    const ryb_status_t rc = ryb_pack_command((uint8_t) RYB_CFG_ADDRESS, first, second, &frame);
    return rc == RYB_OK ? enqueue(frame) : rc;
}
#else
ryb_status_t ryb_send_appended(uint8_t data, ryb_trend_t trend)
{
    uint16_t frame = 0u;
    const ryb_status_t rc = ryb_pack_appended((uint8_t) RYB_CFG_ADDRESS, data, trend, &frame);
    return rc == RYB_OK ? enqueue(frame) : rc;
}
#endif

void ryb_comms_get_stats(ryb_comms_stats_t* out)
{
    if (out == NULL) {
        return;
    }
    out->rx_packets = atomic_load_explicit(&counters.rx_packets, memory_order_relaxed);
    out->rx_parity_errors = atomic_load_explicit(&counters.rx_parity_errors, memory_order_relaxed);
    out->rx_address_errors =
        atomic_load_explicit(&counters.rx_address_errors, memory_order_relaxed);
    out->rx_trend_errors = atomic_load_explicit(&counters.rx_trend_errors, memory_order_relaxed);
    out->rx_overflows = atomic_load_explicit(&counters.rx_overflows, memory_order_relaxed);
    out->tx_packets = atomic_load_explicit(&counters.tx_packets, memory_order_relaxed);
    out->tx_overflows = atomic_load_explicit(&counters.tx_overflows, memory_order_relaxed);
    out->forwarded = atomic_load_explicit(&counters.forwarded, memory_order_relaxed);
}
