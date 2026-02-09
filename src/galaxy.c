#include <rp6502.h>
#include <stdint.h>
#include "galaxy.h"
#include "tables.h"
#include "constants.h"
#include "graphics.h"

// #define N 64 (Replaced by N 80 below)
#define SCALE 60 // Screen scale factor
#define T_INC 25 // 0.2 in 8.8 fixed point

// Globals
static int16_t x, y, t;

// Helper to wrap angle to 0-255 for LUT access
// In Python: sin(i + y) where y is float (scaled by 256 here)
// i is integer 0..63. 1 radian approx 41 units in 0..255 scale (256 / 2PI)
#define RAD_SCALE 41



// RP6502 Color Format: BBBBB GGGGG A RRRRR
// Bits: 15-11 (Blue), 10-6 (Green), 5 (Alpha), 4-0 (Red)
#define COLOR_FROM_RGB8(r,g,b) (((b>>3)<<11)|((g>>3)<<6)|(r>>3))
#define COLOR_ALPHA_MASK (1u<<5)

static void setup_palette(void) {
    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    
    // Synthwave Palette (Bit-Masked Mixing)
    // High Nibble (0xF0): Pink Channel (0..15)
    // Low Nibble  (0x0F): Cyan Channel (0..15)
    
    // R = Pink * 17
    // B = Cyan * 17
    // G = (Pink*8 + Cyan*8) (Mixes to white)
    
    for (int i = 0; i < 256; i++) {
        uint8_t pink = (i >> 4) & 0x0F;
        uint8_t cyan = i & 0x0F;
        
        // Scale 0..15 to 0..255
        // x * 17 covers 0..255 exactly (15*17 = 255)
        
        // Red Channel: Driven ONLY by Pink (High Nibble)
        uint8_t r = pink * 17;
        
        // Green Channel: Driven ONLY by Cyan (Low Nibble)
        // (Previously we added Pink to Green, which made it Orange/Gold. Removed that.)
        uint8_t g = cyan * 17;
        
        // Blue Channel: Driven by BOTH
        // Pink needs Blue to be Magenta (R+B). 
        // Cyan needs Blue to be Cyan (G+B).
        // Standard additive mixing.
        uint16_t b_sum = (pink * 17) + (cyan * 17);
        uint8_t b = (b_sum > 255) ? 255 : (uint8_t)b_sum;
        
        // Convert to RP6502 format with Alpha set
        uint16_t color = COLOR_FROM_RGB8(r, g, b) | COLOR_ALPHA_MASK;
        
        // Write Little Endian
        RIA.rw0 = color & 0xFF;
        RIA.rw0 = (color >> 8) & 0xFF;
    }
}

void galaxy_init(void)
{
    // Initialize variables
    x = 0;
    y = 0;
    t = 0;
    
    setup_palette();
    
    // Clear screen
    RIA.addr0 = PIXEL_DATA_ADDR;
    RIA.step0 = 1;
    for (uint16_t k = 0; k < BITMAP_SIZE; k++) {
        RIA.rw0 = 0;
    }
}

#define N 80 // Increased density

// Stride for decay to save CPU cycles
// Decay 1/2 of the screen per frame in a rolling pattern
static uint8_t decay_pass = 0;

void galaxy_update(void)
{
    uint16_t start_idx = decay_pass; 
    RIA.addr0 = PIXEL_DATA_ADDR + start_idx;
    RIA.step0 = 2; // Skip 2 pixels at a time (Process 50%)
    
    // Bit-Masked Decay
    for (uint16_t i = 0; i < (BITMAP_SIZE / 2); i++) {
        // Read
        uint8_t val = RIA.rw0;
        
        if (val > 0) {
            uint8_t pink = (val >> 4) & 0x0F;
            uint8_t cyan = val & 0x0F;
            
            // Independent Channel Decay
            // Decay by 5 (gone in 3 frames from max 15)
            
            if (pink <= 5) pink = 0;
            else pink -= 5;
            
            if (cyan <= 5) cyan = 0;
            else cyan -= 5;
            
            val = (pink << 4) | cyan;
            
            // Write back
            RIA.addr0 -= 2; // Backup
            RIA.rw0 = val;  // Write
        }
    }
    
    decay_pass = (decay_pass + 1) & 1; // 0, 1

    // Increment time
    if (t > 1608) t -= 1608; 
    t += T_INC;

    int16_t i, j;
    int16_t u, v;
    int16_t screen_x, screen_y;
    
    for (i = 0; i < N; i++) {
        uint8_t ri_idx = (i * 4); 
        uint8_t i_rad_idx = (uint8_t)(((int32_t)i * RAD_SCALE)); // i converted to rad index
        
        for (j = 0; j < N; j++) {
            // u = sin(i+y)+sin(r*i+x)
            
            // Convert y (8.8) to LUT index
            uint8_t y_idx = (uint8_t)(((int32_t)y * RAD_SCALE) >> 8);
            uint8_t x_idx = (uint8_t)(((int32_t)x * RAD_SCALE) >> 8);
            
            uint8_t idx_u1 = i_rad_idx + y_idx;
            uint8_t idx_u2 = ri_idx + x_idx;
            
            u = SIN_LUT[idx_u1] + SIN_LUT[idx_u2]; // Range -512..512 (2.0 in 8.8)
            
            // v = cos(i+y)+cos(r*i+x)
            // cos(phi) = sin(phi + PI/2) = sin(idx + 64)
            
            v = SIN_LUT[(uint8_t)(idx_u1 + 64)] + SIN_LUT[(uint8_t)(idx_u2 + 64)];
            
            // x = u + t
            x = u + t;
            y = v; // y is just v
            
            // Draw
            // circle(u*N/2+W/2, y*N/2+W/2, 2)
            // our u is 8.8 fixed point. 
            // Screen pos = (u * SCALE) >> 8 + Center
            
            screen_x = ((u * SCALE) >> 8) + (SCREEN_WIDTH / 2);
            screen_y = ((v * SCALE) >> 8) + (SCREEN_HEIGHT / 2);
            
            // Draw 3x3 Blur Patch
            // Center is bright, outer ring is faint
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int16_t px = screen_x + dx;
                    int16_t py = screen_y + dy;
                    
                    // Clip
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                        // Bit-Masked Mixing
                        uint16_t addr = (uint16_t)px + (SCREEN_WIDTH * (uint16_t)py);
                        
                        // Determine Particle Stream (Pink or Cyan)
                        uint8_t is_pink = (i < (N/2));
                        
                        // Determine Brightness Addition
                        // Center (0,0) = High (+12)
                        // Outer Ring = Low (+4)
                        uint8_t add_amount = (dx == 0 && dy == 0) ? 12 : 4;
                        
                        // Read current pixel
                        RIA.addr0 = addr;
                        RIA.step0 = 0; 
                        uint8_t old_val = RIA.rw0;
                        
                        // Deconstruct
                        uint8_t pink = (old_val >> 4) & 0x0F;
                        uint8_t cyan = old_val & 0x0F;
                        
                        // Add Brightness
                        if (is_pink) {
                            if (pink < (15 - add_amount)) pink += add_amount;
                            else pink = 15;
                        } else {
                            if (cyan < (15 - add_amount)) cyan += add_amount;
                            else cyan = 15;
                        }
                        
                        // Reconstruct
                        uint8_t new_val = (pink << 4) | cyan;
                        
                        // Write back
                        RIA.rw0 = new_val;
                    }
                }
            }
        }
    }
}
