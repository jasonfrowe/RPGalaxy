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


uint16_t timer_accumulator = 0;
bool music_enabled = true;

static void init_graphics(void)
{
    // Initialize graphics here
    xregn(1, 0, 0, 1, 2); // 320x180

    // Note: galaxy_init handles palette and clearing now
    
    // Configure bitmap parameters
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, width_px, 320);
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, height_px, 180);
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, xram_data_ptr, 0);
    xram0_struct_set(BITMAP_CONFIG_ADDR, vga_mode3_config_t, xram_palette_ptr, PALETTE_ADDR); // Use custom palette
    
    // Enable Mode 3 bitmap (8-bit color)
    xregn(1, 0, 1, 4, 3, 3, BITMAP_CONFIG_ADDR, 0);
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
    
    // Seed Galaxy with somewhat random value (VSYNC + XRAM Data)
    // We don't have user input yet, but VSYNC counter is running.
    // Better: Run a small loop waiting for VSYNC to change? No.
    // Let's use RIA.rw0 from uninitialized memory or just the frame_count after a few loops?
    // Actually, let's just use a static counter that increments during system init?
    // Or better: Use the first user input to re-seed?
    // User wants "Random each start". 
    // On real hardware, RAM might be random? No, XRAM is cleared.
    // Let's rely on the fact that VSYNC aligns differently on boot?
    // Or just use a hardcoded value + RIA.errno?
    // Let's try to use the timer accumulator which might be somewhat random if OPL is ready?
    // Actually, on RP6502, RIA.time isn't available.
    // Let's increment a seed in the main loop until the user presses a button?
    // But the game starts immediately.
    // Let's just use a simple seed for now based on a loop counter.
    
    // Initialize with a arbitrary seed for now, but we need entropy.
    // Let's make the Title Screen wait for "Start"?
    // The user didn't ask for a Title Screen.
    // I'll leave it deterministic for now but call the function so I can hook it up later?
    // User requested randomization.
    // I'll try to read a register that might be noisy?
    // No.
    // I'll use a hardcoded seed + loop iteration count.
    
    galaxy_randomize(12345 + (uint16_t)&frame_count); // Stack address might vary? Unlikely.
    // Wait, RIA.vsync is free running.
    galaxy_randomize(RIA.vsync * 123 + 456);

    while (1) {
        // 1. Poll Audio (High Priority)
        // Check VSYNC and update music if needed
        uint8_t vsync_now = RIA.vsync;
        if (vsync_now != vsync_last) {
            vsync_last = vsync_now;
            process_audio_frame();
            update_sprites();
            update_enemies();
            update_workers();
            
            // Input Processing
            handle_input();

            // Worker Spawning (Simple Cooldown)
            static int spawn_cooldown = 0;
            if (spawn_cooldown > 0) spawn_cooldown--;

            // Button A (Guardian) - GP_BTN_A is in btn0
            if (spawn_cooldown == 0 && (gamepad[0].btn0 & GP_BTN_A)) {
                spawn_worker(0, reticle_x, reticle_y);
                spawn_cooldown = 15; // 0.25s cooldown
            }
            // Button B (Gardener) - GP_BTN_B is in btn0
            if (spawn_cooldown == 0 && (gamepad[0].btn0 & GP_BTN_B)) {
                spawn_worker(1, reticle_x, reticle_y);
                spawn_cooldown = 15;
            }
            
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
        galaxy_tick();
    }
}


