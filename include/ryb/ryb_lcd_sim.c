/* Purpose: host stand-in for ryb_lcd.c: there is no display, so module code still builds and runs.
 * Owning module: management (mgmt/). */

#include <stdint.h>

#include "ryb/ryb_lcd.h"

ryb_status_t ryb_lcd_init(void)
{
    return RYB_ERR_IO;
}

void ryb_lcd_close(void)
{
}

void ryb_lcd_show_comms(void)
{
}

void ryb_lcd_clear(uint16_t color)
{
    (void) color;
}

void ryb_lcd_text(uint8_t row, uint8_t col, const char* text, uint16_t fg, uint16_t bg)
{
    (void) row;
    (void) col;
    (void) text;
    (void) fg;
    (void) bg;
}
