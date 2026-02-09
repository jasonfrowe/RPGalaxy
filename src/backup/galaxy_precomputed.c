#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "galaxy_precomputed.h"
#include "graphics.h"
#include "constants.h"

// Delta change format: (old_x, old_y, new_x, new_y, color)
typedef struct {
    uint16_t old_x;
    uint16_t old_y;
    uint16_t new_x;
    uint16_t new_y;
    uint8_t color;
} PixelChange;

#define BUFFER_SIZE 1024  // Large buffer to minimize file I/O
#define PIXELS_PER_FRAME 10  // Minimal pixels per frame for smooth music

static PixelChange buffer[BUFFER_SIZE];
static uint16_t buffer_pos = 0;
static uint16_t buffer_count = 0;
static uint16_t changes_remaining = 0;
static FILE *frame_file = NULL;
static bool first_draw = true;
static uint8_t frame_delay = 0;
static uint8_t frames_per_update = 6; // Advance frame every 6 video frames (10 FPS)

void galaxy_precomputed_init(void) {
    // Open frame data from USB
    frame_file = fopen("galaxy_frames.bin", "rb");
    if (!frame_file) {
        return;
    }
}

static bool load_buffer(void) {
    if (!frame_file) return false;
    
    // How many changes to read?
    uint16_t to_read = BUFFER_SIZE;
    if (changes_remaining < to_read) {
        to_read = changes_remaining;
    }
    
    if (to_read == 0) return false;
    
    // Read chunk into buffer
    size_t count = fread(buffer, sizeof(PixelChange), to_read, frame_file);
    buffer_count = count;
    buffer_pos = 0;
    changes_remaining -= count;
    
    return count > 0;
}

static bool start_next_frame(void) {
    if (!frame_file) return false;
    
    // Read change count for next frame
    uint16_t count;
    if (fread(&count, sizeof(uint16_t), 1, frame_file) != 1) {
        // End of file - loop
        fseek(frame_file, 0, SEEK_SET);
        if (fread(&count, sizeof(uint16_t), 1, frame_file) != 1) {
            return false;
        }
    }
    
    changes_remaining = count;
    buffer_count = 0;
    buffer_pos = 0;
    
    return count > 0;
}

void galaxy_precomputed_draw(void) {
    // One-time screen clear on init
    if (first_draw) {
        RIA.addr0 = 0;
        RIA.step0 = 1;
        for (unsigned i = 0; i < BITMAP_SIZE; i++) {
            RIA.rw0 = 0;
        }
        first_draw = false;
        start_next_frame();
        load_buffer();
    }
    
    if (!frame_file) return;
    
    // Frame rate control
    frame_delay++;
    if (frame_delay >= frames_per_update && changes_remaining == 0 && buffer_pos >= buffer_count) {
        frame_delay = 0;
        start_next_frame();
        load_buffer();
    }
    
    // Draw limited pixels per frame
    RIA.step0 = 1;
    uint16_t drawn = 0;
    
    while (drawn < PIXELS_PER_FRAME) {
        // Need more data?
        if (buffer_pos >= buffer_count) {
            if (changes_remaining > 0) {
                if (!load_buffer()) break;
            } else {
                break; // Frame complete
            }
        }
        
        PixelChange *change = &buffer[buffer_pos++];
        
        // Erase old pixel (if valid)
        if (change->old_x != 0xFFFF) {
            RIA.addr0 = change->old_x + (SCREEN_WIDTH * change->old_y);
            RIA.rw0 = 0;
            drawn++;
        }
        
        // Draw new pixel (if valid)
        if (change->new_x != 0xFFFF && drawn < PIXELS_PER_FRAME) {
            RIA.addr0 = change->new_x + (SCREEN_WIDTH * change->new_y);
            RIA.rw0 = change->color;
            drawn++;
        }
    }
}
