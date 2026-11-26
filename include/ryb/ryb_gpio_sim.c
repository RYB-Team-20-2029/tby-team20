/* Purpose: host GPIO backend: pins joined into wired-AND nets with a pull-up, for laptop tests.
 * Owning module: management (mgmt/). */

#include <stdint.h>

#include "ryb/ryb_gpio.h"

#define SIM_PINS 64u

/* Only the bit loop thread touches these after ryb_comms_init(), and thread creation orders
 * everything written before it, so no atomics are needed. */
static uint8_t net[SIM_PINS];
static uint8_t is_output[SIM_PINS];
static uint8_t level_out[SIM_PINS];
static uint8_t nets_ready;

static void ensure_nets(void)
{
    if (nets_ready == 0u) {
        for (uint8_t i = 0u; i < SIM_PINS; i++) {
            net[i] = i;
        }
        nets_ready = 1u;
    }
}

ryb_status_t ryb_gpio_open(uint32_t base_address)
{
    (void) base_address;
    ensure_nets();
    return RYB_OK;
}

void ryb_gpio_close(void)
{
}

void ryb_gpio_sim_connect(uint8_t a, uint8_t b)
{
    ensure_nets();
    const uint8_t from = net[b % SIM_PINS];
    const uint8_t to = net[a % SIM_PINS];
    for (uint8_t i = 0u; i < SIM_PINS; i++) {
        if (net[i] == from) {
            net[i] = to;
        }
    }
}

void ryb_gpio_set_input(uint8_t bit)
{
    is_output[bit % SIM_PINS] = 0u;
}

void ryb_gpio_set_output(uint8_t bit)
{
    is_output[bit % SIM_PINS] = 1u;
}

void ryb_gpio_release(uint8_t bit)
{
    ryb_gpio_set_input(bit);
}

void ryb_gpio_write(uint8_t bit, uint8_t level)
{
    level_out[bit % SIM_PINS] = (uint8_t) (level != 0u);
}

uint8_t ryb_gpio_read(uint8_t bit)
{
    /* Any output on the net driving 0 pulls it low; otherwise the pull-up wins. */
    const uint8_t self = net[bit % SIM_PINS];
    for (uint8_t i = 0u; i < SIM_PINS; i++) {
        if (net[i] == self && is_output[i] != 0u && level_out[i] == 0u) {
            return 0u;
        }
    }
    return 1u;
}
