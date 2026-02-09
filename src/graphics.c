#include <rp6502.h>
#include <stdint.h>
#include "constants.h"

// Routine for placing a single dot on the screen for 8bit-colour depth
void set(int16_t x, int16_t y, uint8_t colour)
{
    RIA.addr0 = x + (SCREEN_WIDTH * y);
    RIA.step0 = 1;
    RIA.rw0 = colour;
}

// Routine for reading a pixel value from the screen
uint8_t get_pixel(int16_t x, int16_t y)
{
    RIA.addr0 = x + (SCREEN_WIDTH * y);
    RIA.step0 = 1;
    return RIA.rw0;
}