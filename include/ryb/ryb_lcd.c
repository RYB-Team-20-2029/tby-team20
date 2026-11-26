/* Purpose: ST7789 driver for the 1.54" LCD on the PYNQ-Z2 Raspberry Pi header, and the RYB Comms
 * status screen. Owning module: management (mgmt/). */

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ryb/ryb_comms.h"
#include "ryb/ryb_gpio.h"
#include "ryb/ryb_lcd.h"

/* Controllers and set-up as the TU/e image's own display code (libpynq, built into
 * /boot/cat-display) uses them in the io0.2.0 overlay. The pins come from the "lcd" section of
 * ryb_comms.json, as io_switch numbers, which are also the GPIO bit numbers. */
#define SWITCH_BASE 0x42000000u
#define SPI_BASE    0x41E10000u /* axi_quad_spi_1 */
#define PIN_BL      ((uint8_t) RYB_CFG_LCD_BL)
#define PIN_RST     ((uint8_t) RYB_CFG_LCD_RST)
#define PIN_DC      ((uint8_t) RYB_CFG_LCD_DC)
#define PIN_CS      ((uint8_t) RYB_CFG_LCD_CS)
#define PIN_MOSI    ((uint8_t) RYB_CFG_LCD_MOSI)
#define PIN_SCK     ((uint8_t) RYB_CFG_LCD_SCLK)

/* io_switch functions; each 32-bit switch register holds the bytes of four pins. */
#define SWB_GPIO      0x00u
#define SWB_SPI1_SCK  0x08u
#define SWB_SPI1_MOSI 0x0Au
#define SWB_SPI1_SS   0x0Bu

/* AXI Quad SPI registers (Xilinx PG153), as 32-bit word indices. */
#define SPI_SRR        (0x40u / 4u)
#define SPI_CR         (0x60u / 4u)
#define SPI_SR         (0x64u / 4u)
#define SPI_DTR        (0x68u / 4u)
#define SPI_SSR        (0x70u / 4u)
#define SRR_RESET      0x0Au
#define CR_MASTER_MODE3 0x1Eu /* enabled, master, CPOL 1, CPHA 1 */
#define CR_FIFO_RESET  0x60u
#define SR_TX_EMPTY    0x04u
#define SR_TX_FULL     0x08u

/* TX empty is raised while the last byte is still shifting out, and the panel samples the
 * data/command pin with that byte's final bit, so wait this much longer before moving it. */
#define SHIFT_OUT_NS 10000ull
/* A wait on the SPI controller longer than this means it is not there; stop drawing. */
#define SPI_TIMEOUT_NS 50000000ull

#define WIDTH   240u
#define HEIGHT  240u
#define GLYPH_W 8u
#define GLYPH_H 16u

/* FONTX2: a 17-byte header, then 256 glyphs of 16 bytes, one byte per row, MSB leftmost. The
 * TU/e image ships one; it is what /boot/cat-display draws its boot text with. */
#define FONT_PATH   "/boot/ILGH16XB.FNT"
#define FONT_HEADER 17u
#define FONT_BYTES  (FONT_HEADER + 256u * GLYPH_H)

#define ST_SWRESET 0x01u
#define ST_SLPOUT  0x11u
#define ST_NORON   0x13u
#define ST_INVON   0x21u
#define ST_CASET   0x2Au
#define ST_RASET   0x2Bu
#define ST_RAMWR   0x2Cu
#define ST_DISPON  0x29u
#define ST_MADCTL  0x36u
#define ST_COLMOD  0x3Au

/* A packet older than this is shown in red: its sender has probably stopped. Three missed sends
 * at the monitor's period, but never under 2 s. */
#define STALE_MONITOR_NS ((uint64_t) RYB_CFG_MONITOR_PERIOD_MS * 3000000ull)
#define STALE_NS         (STALE_MONITOR_NS > 2000000000ull ? STALE_MONITOR_NS : 2000000000ull)

typedef struct {
    char ch;
    uint16_t fg;
    uint16_t bg;
} cell_t;

static volatile uint32_t* spi;
static volatile uint32_t* sw;
static int mem_fd = -1;
static bool gpio_held;
static bool broken;
static uint8_t font[FONT_BYTES];
/* What is on the panel, so only changed characters are sent. */
static cell_t screen[RYB_LCD_ROWS][RYB_LCD_COLS];

static const char* const sender_names[] = RYB_CFG_SENDER_NAMES;
static const char* const value_names[] = RYB_CFG_VALUE_NAMES;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static void sleep_ms(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t) (ms / 1000u);
    ts.tv_nsec = (long) (ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}

/* Wait until the SPI status register shows `bits` as `want`. */
static bool spi_wait(uint32_t bits, uint32_t want)
{
    if (broken) {
        return false;
    }
    if ((spi[SPI_SR] & bits) == want) {
        return true;
    }
    const uint64_t deadline = now_ns() + SPI_TIMEOUT_NS;
    while ((spi[SPI_SR] & bits) != want) {
        if (now_ns() > deadline) {
            fprintf(stderr, "ryb_lcd: SPI controller not responding, display disabled\n");
            broken = true;
            return false;
        }
    }
    return true;
}

static void spi_put(uint8_t byte)
{
    if (spi_wait(SR_TX_FULL, 0u)) {
        spi[SPI_DTR] = byte;
    }
}

static void spi_flush(void)
{
    if (spi_wait(SR_TX_EMPTY, SR_TX_EMPTY)) {
        const uint64_t until = now_ns() + SHIFT_OUT_NS;
        while (now_ns() < until) {
        }
    }
}

static void command(uint8_t cmd)
{
    spi_flush();
    ryb_gpio_write(PIN_DC, 0u);
    spi_put(cmd);
    spi_flush();
    ryb_gpio_write(PIN_DC, 1u);
}

static void put16(uint16_t v)
{
    spi_put((uint8_t) (v >> 8));
    spi_put((uint8_t) (v & 0xFFu));
}

/* Address a rectangle; the pixels that follow fill it row by row. */
static void window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    command(ST_CASET);
    put16(x0);
    put16(x1);
    command(ST_RASET);
    put16(y0);
    put16(y1);
    command(ST_RAMWR);
}

static void route(uint8_t pin, uint8_t function)
{
    const unsigned shift = (pin % 4u) * 8u;
    const uint32_t v = sw[pin / 4u];
    sw[pin / 4u] = (v & ~(0xFFu << shift)) | ((uint32_t) function << shift);
}

static volatile uint32_t* map_page(uint32_t base)
{
    void* p = mmap(NULL, 0x1000u, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t) base);
    return p == MAP_FAILED ? NULL : (volatile uint32_t*) p;
}

static bool load_font(void)
{
    FILE* f = fopen(FONT_PATH, "rb");
    if (f == NULL) {
        perror("ryb_lcd: " FONT_PATH);
        return false;
    }
    const size_t n = fread(font, 1u, sizeof font, f);
    fclose(f);
    if (n != sizeof font || memcmp(font, "FONTX2", 6u) != 0 || font[14] != GLYPH_W ||
        font[15] != GLYPH_H) {
        fprintf(stderr, "ryb_lcd: " FONT_PATH " is not an 8x16 FONTX2 font\n");
        return false;
    }
    return true;
}

/* Send columns c0..c1 of a text row, from the screen buffer. */
static void draw_span(uint8_t row, uint8_t c0, uint8_t c1)
{
    window((uint16_t) (c0 * GLYPH_W), (uint16_t) (row * GLYPH_H),
           (uint16_t) ((c1 + 1u) * GLYPH_W - 1u), (uint16_t) (row * GLYPH_H + GLYPH_H - 1u));
    for (uint8_t y = 0u; y < GLYPH_H; y++) {
        for (uint8_t c = c0; c <= c1; c++) {
            const cell_t* cell = &screen[row][c];
            const uint8_t bits = font[FONT_HEADER + (uint8_t) cell->ch * GLYPH_H + y];
            for (int b = 7; b >= 0; b--) {
                put16(((bits >> b) & 1u) != 0u ? cell->fg : cell->bg);
            }
        }
    }
}

void ryb_lcd_text(uint8_t row, uint8_t col, const char* text, uint16_t fg, uint16_t bg)
{
    if (spi == NULL || broken || row >= RYB_LCD_ROWS || text == NULL) {
        return;
    }
    int first = -1;
    int last = -1;
    for (uint8_t c = col; c < RYB_LCD_COLS && *text != '\0'; c++, text++) {
        cell_t* cell = &screen[row][c];
        if (cell->ch != *text || cell->fg != fg || cell->bg != bg) {
            cell->ch = *text;
            cell->fg = fg;
            cell->bg = bg;
            if (first < 0) {
                first = c;
            }
            last = c;
        }
    }
    if (first >= 0) {
        draw_span(row, (uint8_t) first, (uint8_t) last);
    }
}

void ryb_lcd_clear(uint16_t color)
{
    if (spi == NULL || broken) {
        return;
    }
    window(0u, 0u, WIDTH - 1u, HEIGHT - 1u);
    for (uint32_t i = 0u; i < WIDTH * HEIGHT; i++) {
        put16(color);
    }
    for (uint8_t r = 0u; r < RYB_LCD_ROWS; r++) {
        for (uint8_t c = 0u; c < RYB_LCD_COLS; c++) {
            screen[r][c].ch = ' ';
            screen[r][c].fg = color;
            screen[r][c].bg = color;
        }
    }
}

ryb_status_t ryb_lcd_init(void)
{
    if (spi != NULL) {
        return RYB_OK;
    }
    if (!RYB_CFG_HAS_LCD) {
        fprintf(stderr, "ryb_lcd: ryb_comms.json has no \"lcd\" section\n");
        return RYB_ERR_STATE;
    }
    if (!load_font()) {
        return RYB_ERR_IO;
    }
    const ryb_status_t rc = ryb_gpio_open(RYB_CFG_GPIO_BASE);
    if (rc != RYB_OK) {
        return rc;
    }
    gpio_held = true;
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    sw = mem_fd >= 0 ? map_page(SWITCH_BASE) : NULL;
    spi = mem_fd >= 0 ? map_page(SPI_BASE) : NULL;
    if (sw == NULL || spi == NULL) {
        perror("ryb_lcd: mapping the switch and SPI controller");
        ryb_lcd_close();
        return RYB_ERR_IO;
    }
    broken = false;

    route(PIN_MOSI, SWB_SPI1_MOSI);
    route(PIN_SCK, SWB_SPI1_SCK);
    route(PIN_CS, SWB_SPI1_SS);
    route(PIN_BL, SWB_GPIO);
    route(PIN_RST, SWB_GPIO);
    route(PIN_DC, SWB_GPIO);
    ryb_gpio_write(PIN_BL, 0u);
    ryb_gpio_write(PIN_DC, 1u);
    ryb_gpio_write(PIN_RST, 1u);
    ryb_gpio_set_output(PIN_BL);
    ryb_gpio_set_output(PIN_DC);
    ryb_gpio_set_output(PIN_RST);

    spi[SPI_SRR] = SRR_RESET;
    spi[SPI_CR] = CR_MASTER_MODE3 | CR_FIFO_RESET;
    spi[SPI_SSR] = 0u;

    /* Reset pulse and panel set-up, timings as in the TU/e driver. */
    sleep_ms(100u);
    ryb_gpio_write(PIN_RST, 0u);
    sleep_ms(500u);
    ryb_gpio_write(PIN_RST, 1u);
    sleep_ms(300u);
    command(ST_SWRESET);
    sleep_ms(150u);
    command(ST_SLPOUT);
    sleep_ms(255u);
    command(ST_COLMOD);
    spi_put(0x55u); /* 16-bit RGB565 */
    sleep_ms(10u);
    command(ST_MADCTL);
    spi_put(0x00u);
    command(ST_INVON);
    sleep_ms(10u);
    command(ST_NORON);
    sleep_ms(10u);
    command(ST_DISPON);
    sleep_ms(255u);
    ryb_gpio_write(PIN_BL, 1u);

    ryb_lcd_clear(RYB_LCD_BLACK);
    if (broken) {
        ryb_lcd_close();
        return RYB_ERR_IO;
    }
    return RYB_OK;
}

void ryb_lcd_close(void)
{
    if (spi != NULL) {
        spi_flush();
        munmap((void*) spi, 0x1000u);
        spi = NULL;
    }
    if (sw != NULL) {
        munmap((void*) sw, 0x1000u);
        sw = NULL;
    }
    if (mem_fd >= 0) {
        close(mem_fd);
        mem_fd = -1;
    }
    if (gpio_held) {
        ryb_gpio_close();
        gpio_held = false;
    }
}


/* Text in `width` cells from `col`, padded with spaces so leftovers from longer text are wiped.
 * Right-aligned when `right` is set. */
static void field(uint8_t row, uint8_t col, uint8_t width, bool right, uint16_t fg, uint16_t bg,
                  const char* fmt, ...)
{
    char text[RYB_LCD_COLS + 1u];
    char cells[RYB_LCD_COLS + 1u];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    if (n < 0) {
        n = 0;
    }
    if (n > width) {
        n = width;
    }
    memset(cells, ' ', width);
    memcpy(cells + (right ? width - n : 0), text, (size_t) n);
    cells[width] = '\0';
    ryb_lcd_text(row, col, cells, fg, bg);
}

static const char* trend_name(ryb_trend_t t)
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

/* The frame's wire bits, split at the field boundaries of its packet type. */
static void frame_bits(char* out, const ryb_comms_sender_t* s)
{
    static const uint8_t appended[] = {2u, 8u, 2u, 1u};
    static const uint8_t command_fields[] = {2u, 5u, 5u, 1u};
    const uint8_t* widths = s->packet.kind == RYB_PACKET_COMMAND ? command_fields : appended;
    int bit = 12;
    for (int f = 0; f < 4; f++) {
        for (uint8_t i = 0u; i < widths[f]; i++, bit--) {
            *out++ = ((s->frame >> bit) & 1u) != 0u ? '1' : '0';
        }
        *out++ = f < 3 ? ' ' : '\0';
    }
}

/* Three rows for one address: who, how many and how long ago; the decoded fields; the bits. */
static void show_sender(uint8_t row, uint8_t address, const char* label, uint64_t now)
{
    const uint8_t r1 = (uint8_t) (row + 1u);
    const uint8_t r2 = (uint8_t) (row + 2u);
    ryb_comms_sender_t s;
    ryb_comms_get_sender(address, &s);

    if (s.count == 0u) {
        field(row, 0u, RYB_LCD_COLS, false, RYB_LCD_CYAN, RYB_LCD_BLACK, "%-9s %u%u  nothing yet",
              label, (unsigned) (address >> 1), (unsigned) (address & 1u));
        field(r1, 0u, RYB_LCD_COLS, false, RYB_LCD_WHITE, RYB_LCD_BLACK, "%s", "");
        field(r2, 0u, RYB_LCD_COLS, false, RYB_LCD_GREY, RYB_LCD_BLACK, "%s", "");
        return;
    }

    const uint64_t age = now - s.t_last_ns;
    const unsigned tenths = (unsigned) (age / 100000000ull);
    field(row, 0u, RYB_LCD_COLS - 6u, false, RYB_LCD_CYAN, RYB_LCD_BLACK, "%-9s %u%u  #%lu", label,
          (unsigned) (address >> 1), (unsigned) (address & 1u), (unsigned long) s.count);
    /* Coloured on its own, so a sender that has gone quiet stands out. */
    const uint16_t age_color = age > STALE_NS ? RYB_LCD_RED : RYB_LCD_GREEN;
    if (tenths > 999u) {
        field(row, RYB_LCD_COLS - 6u, 6u, true, age_color, RYB_LCD_BLACK, "%s", ">99s");
    } else {
        field(row, RYB_LCD_COLS - 6u, 6u, true, age_color, RYB_LCD_BLACK, "%u.%us", tenths / 10u,
              tenths % 10u);
    }

    if (s.packet.kind == RYB_PACKET_COMMAND) {
        field(r1, 0u, RYB_LCD_COLS, false, RYB_LCD_WHITE, RYB_LCD_BLACK, "  first %u  second %u",
              (unsigned) s.packet.u.command.first, (unsigned) s.packet.u.command.second);
    } else {
        field(r1, 0u, RYB_LCD_COLS, false, RYB_LCD_WHITE, RYB_LCD_BLACK, "  %s %u  trend %s",
              value_names[address], (unsigned) s.packet.u.appended.data,
              trend_name(s.packet.u.appended.trend));
    }
    char bits[20];
    frame_bits(bits, &s);
    field(r2, 0u, RYB_LCD_COLS, false, RYB_LCD_GREY, RYB_LCD_BLACK, "  %s", bits);
}

void ryb_lcd_show_comms(void)
{
    if (spi == NULL || broken) {
        return;
    }
    const uint64_t now = now_ns();
    ryb_comms_stats_t st;
    ryb_comms_get_stats(&st);

    field(0u, 0u, RYB_LCD_COLS, false, RYB_LCD_WHITE, RYB_LCD_BLUE, " RYB %s  addr %u%u  %uus",
          RYB_CFG_MODULE_NAME, (unsigned) (RYB_CFG_ADDRESS >> 1),
          (unsigned) (RYB_CFG_ADDRESS & 1u), (unsigned) RYB_CFG_BIT_PERIOD_US);
    field(1u, 0u, RYB_LCD_COLS, false, RYB_LCD_GREY, RYB_LCD_BLACK, "rx %lu  err %lu/%lu/%lu  tx %lu",
          (unsigned long) st.rx_packets, (unsigned long) st.rx_parity_errors,
          (unsigned long) st.rx_address_errors, (unsigned long) st.rx_trend_errors,
          (unsigned long) st.tx_packets);

    /* Blocks of three rows: this module's last send, then each sender it listens to. The last
     * row is kept for warnings. */
    uint8_t row = 2u;
    if (RYB_CFG_TX_BIT >= 0) {
        show_sender(row, (uint8_t) RYB_CFG_ADDRESS, "sent", now);
        row = (uint8_t) (row + 3u);
    }
    for (uint8_t a = 0u; a <= RYB_ADDR_MAX && row + 3u < RYB_LCD_ROWS; a++) {
        if ((RYB_CFG_ACCEPT_MASK & (1u << a)) != 0u && a != RYB_CFG_ADDRESS) {
            show_sender(row, a, sender_names[a], now);
            row = (uint8_t) (row + 3u);
        }
    }
    /* Bottom row: this board's wiring, so a glance tells whether it is configured right. */
    if (st.rx_overflows != 0u || st.tx_overflows != 0u) {
        field(RYB_LCD_ROWS - 1u, 0u, RYB_LCD_COLS, false, RYB_LCD_YELLOW, RYB_LCD_BLACK,
              "dropped: rx %lu tx %lu", (unsigned long) st.rx_overflows,
              (unsigned long) st.tx_overflows);
    } else {
        field(RYB_LCD_ROWS - 1u, 0u, RYB_LCD_COLS, false, RYB_LCD_GREY, RYB_LCD_BLACK,
              "tx %s  rx %s", RYB_CFG_TX_NAME, RYB_CFG_RX_NAMES);
    }
}
