/* Purpose: PYNQ-Z2 GPIO backend: the AXI GPIO behind the Arduino header, mapped through /dev/mem.
 * Owning module: management (mgmt/). */

#include <fcntl.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "ryb/ryb_gpio.h"

/* AXI GPIO register map (Xilinx PG144) in 32-bit words: data, then direction, per channel.
 * A direction bit of 1 makes that pin an input. */
#define REG_DATA(bit) (((bit) / 32u) * 2u)
#define REG_TRI(bit)  (((bit) / 32u) * 2u + 1u)
#define MASK(bit)     (1u << ((bit) % 32u))

/* The guideline maps exactly one page at the controller's base. */
#define MAP_BYTES 0x1000u

static volatile uint32_t* regs = NULL;
static int mem_fd = -1;
static int users;
static atomic_flag busy = ATOMIC_FLAG_INIT;

static void lock(void)
{
    while (atomic_flag_test_and_set_explicit(&busy, memory_order_acquire)) {
    }
}

static void unlock(void)
{
    atomic_flag_clear_explicit(&busy, memory_order_release);
}

ryb_status_t ryb_gpio_open(uint32_t base_address)
{
    if (regs != NULL) {
        users++;
        return RYB_OK;
    }
    if ((base_address & (MAP_BYTES - 1u)) != 0u) {
        fprintf(stderr, "ryb_gpio: base address 0x%08lx is not page aligned\n",
                (unsigned long) base_address);
        return RYB_ERR_PARAM;
    }

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("ryb_gpio: open /dev/mem (needs root)");
        return RYB_ERR_IO;
    }

    void* map = mmap(NULL, MAP_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd,
                     (off_t) base_address);
    if (map == MAP_FAILED) {
        perror("ryb_gpio: mmap");
        close(mem_fd);
        mem_fd = -1;
        return RYB_ERR_IO;
    }
    regs = (volatile uint32_t*) map;
    users = 1;
    return RYB_OK;
}

void ryb_gpio_close(void)
{
    if (regs == NULL || --users > 0) {
        return;
    }
    munmap((void*) regs, MAP_BYTES);
    regs = NULL;
    close(mem_fd);
    mem_fd = -1;
}

void ryb_gpio_set_input(uint8_t bit)
{
    lock();
    regs[REG_TRI(bit)] |= MASK(bit);
    unlock();
}

void ryb_gpio_set_output(uint8_t bit)
{
    lock();
    regs[REG_TRI(bit)] &= ~MASK(bit);
    unlock();
}

void ryb_gpio_write(uint8_t bit, uint8_t level)
{
    lock();
    if (level != 0u) {
        regs[REG_DATA(bit)] |= MASK(bit);
    } else {
        regs[REG_DATA(bit)] &= ~MASK(bit);
    }
    unlock();
}

uint8_t ryb_gpio_read(uint8_t bit)
{
    return (uint8_t) ((regs[REG_DATA(bit)] & MASK(bit)) != 0u);
}

void ryb_gpio_release(uint8_t bit)
{
    if (regs != NULL) {
        regs[REG_TRI(bit)] |= MASK(bit);
    }
}
