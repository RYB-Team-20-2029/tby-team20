/* Purpose: the 1.54" LCD on the PYNQ-Z2's Raspberry Pi header: text, and a screen of the packets
 * RYB Comms receives. Owning module: management (mgmt/). */

#ifndef RYB_LCD_H
#define RYB_LCD_H

#include <stdint.h>

#include "ryb/ryb_packet.h"

/* 240 x 240 pixels, used as 15 rows of 30 characters, each 8 x 16. */
#define RYB_LCD_ROWS 15u
#define RYB_LCD_COLS 30u

/* RGB565 colours. */
#define RYB_LCD_BLACK  0x0000u
#define RYB_LCD_WHITE  0xFFFFu
#define RYB_LCD_GREY   0x8410u
#define RYB_LCD_RED    0xF800u
#define RYB_LCD_GREEN  0x07E0u
#define RYB_LCD_BLUE   0x001Fu
#define RYB_LCD_YELLOW 0xFFE0u
#define RYB_LCD_CYAN   0x07FFu

/* Route the display pins, reset and set up the panel, and load the font. Takes about 1.6 s,
 * almost all of it the panel's own reset and wake-up delays. Needs root, like ryb_comms_init().
 * On the host (sim backend) there is no display: it returns RYB_ERR_IO and the drawing calls do
 * nothing, so a module should carry on without it. */
ryb_status_t ryb_lcd_init(void);
void ryb_lcd_close(void);

/* Draw the RYB Comms screen. For each sender this module listens to: the address, value and
 * trend of its last packet, its raw bits, how many arrived and how long ago. Above them the link
 * counters and the last packet this module sent. Only characters that changed are redrawn, so
 * calling it a few times a second from the main loop is cheap. It blocks for the SPI transfers,
 * so never call it from a receive callback; the bit loop has its own thread and is unaffected. */
void ryb_lcd_show_comms(void);

/* Building blocks for a module's own screen. Text is clipped at the right edge. */
void ryb_lcd_clear(uint16_t color);
void ryb_lcd_text(uint8_t row, uint8_t col, const char* text, uint16_t fg, uint16_t bg);

#endif /* RYB_LCD_H */
