#include <rp6502.h>
#include <stdint.h>
#include <math.h> 

#include "sprites.h"
#include "tables.h"
#include "constants.h"

#define SPRITE_CONFIG_ADDR 0xFD00
#define SPRITE_DATA_ADDR   0xF000 // In XRAM (mapped from ROM 0x1F000)

static uint16_t sprite_angle = 0; // 0..255
static uint16_t pulse_time = 0;
int16_t reticle_x = 144; // Start center
int16_t reticle_y = 74;

void init_sprites(void)
{
    // Define the struct in XRAM first
    RIA.addr0 = SPRITE_CONFIG_ADDR;
    RIA.step0 = 1;

    // Use vga_mode4_asprite_t (Affine Sprite)
    // Initialize with identity transform (Scale 1.0 = 256)
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[0], 256); // SX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[1], 0);   // SHY
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[2], 0);   // TX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[3], 0);   // SHX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[4], 256); // SY
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[5], 0);   // TY

    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, x_pos_px, reticle_x);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, y_pos_px, reticle_y);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, xram_sprite_ptr, SPRITE_DATA_ADDR);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, log_size, 5); // 32x32 = 2^5
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, has_opacity_metadata, false);

    // Enable Plane 0 for Sprites (Mode 4) - Top Layer
    // Reg 1: Mode = 4
    // Reg 2: Options = 1 
    // Reg 3: Config_Ptr = SPRITE_CONFIG_ADDR
    // Reg 4: Length = 1
    // Reg 5: Plane = 0 (TOP)
    xregn(1, 0, 1, 4, 4, 1, SPRITE_CONFIG_ADDR, 1, 2); // Changed from Plane 1 to Plane 2
}

void update_reticle_position(int8_t dx, int8_t dy) {
    reticle_x += dx;
    reticle_y += dy;
    
    // Bounds Check (screen is 320x180)
    // Keep the box somewhat on screen
    if (reticle_x < -16) reticle_x = -16;
    if (reticle_x > 320-16) reticle_x = 320-16;
    if (reticle_y < -16) reticle_y = -16;
    if (reticle_y > 180-16) reticle_y = 180-16;
}

void update_sprites(void)
{
    sprite_angle += 1; // 1 degree per frame (256 = 360 approx)
    pulse_time += 4;
    
    // Scale: 1.0 (256) +/- 0.2 (50)
    // sin returns -256..256 (1.0 in 8.8)
    int16_t s_val = SIN_LUT[pulse_time & 0xFF]; // -256..256
    // We want output varying by +/- 50. s_val/5?
    int16_t scale = 256 + (s_val / 5); 
    
    // Angle
    uint8_t angle = (uint8_t)sprite_angle;
    int16_t c = SIN_LUT[(uint8_t)(angle + 64)]; // cos
    int16_t s = SIN_LUT[angle]; // sin
    
    // Affine Matrix 8.8 Fixed Point
    // A = sx * cos(theta)
    // B = sy * -sin(theta)
    // C = sx * sin(theta)
    // D = sy * cos(theta)
    
    // SIN_LUT is 1.0 amplitude (256). Scale is 256 (1.0).
    // result = (scale * sin) >> 8
    
    int16_t A = ((int32_t)scale * c) >> 8; 
    int16_t B = ((int32_t)scale * (-s)) >> 8;
    int16_t C = ((int32_t)scale * s) >> 8;
    int16_t D = ((int32_t)scale * c) >> 8;

    // Centering Logic
    // Screen Position: Top-Left at (reticle_x, reticle_y).
    // Pivot: Map Box Center (16,16) to Texture Center (16,16).

    int16_t cx = 16;
    int16_t cy = 16;
    int32_t center_fixed = 16 << 8; // 4096
    
    // TX = 4096 - (A*16 + B*16)
    int16_t TX = (int16_t)(center_fixed - ((int32_t)A * cx) - ((int32_t)B * cy));
    int16_t TY = (int16_t)(center_fixed - ((int32_t)C * cx) - ((int32_t)D * cy));
    
    // Update XRAM Struct
    RIA.addr0 = SPRITE_CONFIG_ADDR;
    RIA.step0 = 1;
    
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[0], A);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[1], B);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[2], TX);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[3], C); 
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[4], D); 
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[5], TY);
    
    // Update Position Dynamically
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, x_pos_px, reticle_x);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, y_pos_px, reticle_y);
}
