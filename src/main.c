#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "constants.h"
#include "opl.h"
#include "instruments.h"
#include "galaxy.h"
#include "sprites.h"
#include "input.h"

#define SONG_HZ 60

unsigned BITMAP_CONFIG;
uint16_t timer_accumulator = 0;
bool music_enabled = true;

static void init_graphics(void)
{
    // Initialize graphics here
    xregn(1, 0, 0, 1, 2); // 320x180

    BITMAP_CONFIG = BITMAP_DATA;
    
    // Note: galaxy_init handles palette and clearing now
    
    // Configure bitmap parameters
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, width_px, 320);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, height_px, 180);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_data_ptr, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_palette_ptr, PALETTE_ADDR); // Use custom palette
    
    // Enable Mode 3 bitmap (8-bit color)
    xregn(1, 0, 1, 4, 3, 3, BITMAP_CONFIG, 0);
}

void init_all_systems(void) {
    // Hardware Setup - Input
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_graphics();

    // Audio Setup
    OPL_Config(1, OPL_ADDR);
    opl_init();
    music_init(MUSIC_FILENAME);
    
    // Galaxy Setup
    galaxy_init();
    init_input_system();
    init_sprites();
}

void process_audio_frame(void) {
    if (!music_enabled) return;
    
    timer_accumulator += SONG_HZ;
    while (timer_accumulator >= 60) {
        update_music();
        timer_accumulator -= 60;
    }
}

int main(void)
{
    init_all_systems();

    uint8_t vsync_last = RIA.vsync;

    int frame_count = 0;

    while (1) {
        // 1. Poll Audio (High Priority)
        // Check VSYNC and update music if needed
        uint8_t vsync_now = RIA.vsync;
        if (vsync_now != vsync_last) {
            vsync_last = vsync_now;
            process_audio_frame();
            update_sprites();
            
            // Input Processing
            handle_input();
            
            int8_t dx = 0;
            int8_t dy = 0;
            // Digital Controls
            if (is_action_pressed(0, ACTION_LEFT)) dx -= 2;
            if (is_action_pressed(0, ACTION_RIGHT)) dx += 2;
            if (is_action_pressed(0, ACTION_UP)) dy -= 2;
            if (is_action_pressed(0, ACTION_DOWN)) dy += 2;
            
            // Analog Controls
            if (dx == 0 && dy == 0) {
                if (abs(gamepad[0].lx) > 10) dx = gamepad[0].lx / 16;
                if (abs(gamepad[0].ly) > 10) dy = gamepad[0].ly / 16;
            }

            update_reticle_position(dx, dy);

            frame_count++;
        }
        
        // 2. Poll Simulation (Low Priority)
        // Run one small slice of the simulation
        // The galaxy_tick function does a tiny amount of work (256 pixels or 8 interactions)
        // and returns immediately.
        galaxy_tick();
    }
}


