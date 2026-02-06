#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "opl.h"
#include "instruments.h"
#include "galaxy_precomputed.h"

#define SONG_HZ 60

unsigned BITMAP_CONFIG;
uint16_t timer_accumulator = 0;
bool music_enabled = true;

static void init_graphics(void)
{
    // Initialize graphics here
    xregn(1, 0, 0, 1, 2); // 320x180

    BITMAP_CONFIG = BITMAP_DATA;

    // Configure bitmap parameters
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, width_px, 320);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, height_px, 180);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_data_ptr, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_palette_ptr, 0xFFFF); // Use built in palette
    
    // Enable Mode 3 bitmap (8-bit color)
    xregn(1, 0, 1, 4, 3, 3, BITMAP_CONFIG, 1);

    // Clear bitmap memory
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = 0; i < BITMAP_SIZE; i++) {
        RIA.rw0 = 0;
    }

}

void init_all_systems(void) {
    init_graphics();

    // Audio Setup
    OPL_Config(1, OPL_ADDR);
    opl_init();
    music_init(MUSIC_FILENAME);
    
    // Galaxy Setup
    galaxy_precomputed_init();

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

    while (1) {
        uint8_t vsync_now = RIA.vsync;
        if (vsync_now != vsync_last) {
            vsync_last = vsync_now;
            
            // AUDIO
            process_audio_frame();
            
            // VISUALS
            galaxy_precomputed_draw();

        }
    }

}
