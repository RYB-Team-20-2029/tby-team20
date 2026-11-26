/* Purpose: the few GPIO operations RYB Comms needs, so the bit loop never touches a register.
 * Owning module: management (mgmt/). Backends: ryb_gpio_mmio.c (PYNQ), ryb_gpio_sim.c (host). */

#ifndef RYB_GPIO_H
#define RYB_GPIO_H

#include <stdint.h>

#include "ryb/ryb_packet.h"

/* `bit` is a GPIO bit number, 0..63, as resolved from the "pins" table in ryb_comms.json.
 * Bits 32..63 sit on the controller's second channel. */

/* Counted: the bit loop and the LCD each open it, and the mapping goes when both have closed. */
ryb_status_t ryb_gpio_open(uint32_t base_address);
void ryb_gpio_close(void);

/* The writes are read-modify-write under a short lock, because the LCD's control pins share
 * registers with the comms pins and are written from another thread. An input is also how an
 * open-drain sender releases the line to its pull-up. */
void ryb_gpio_set_input(uint8_t bit);
void ryb_gpio_set_output(uint8_t bit);
void ryb_gpio_write(uint8_t bit, uint8_t level);
uint8_t ryb_gpio_read(uint8_t bit);

/* ryb_gpio_set_input() without the lock, for a signal handler that may have interrupted its
 * holder. Only for releasing a line on the way out. */
void ryb_gpio_release(uint8_t bit);

/* Sim backend only: join two pins into one wired-AND net with a pull-up, standing in for a
 * backbone conductor. Call before ryb_comms_init(). */
void ryb_gpio_sim_connect(uint8_t a, uint8_t b);

#endif /* RYB_GPIO_H */
