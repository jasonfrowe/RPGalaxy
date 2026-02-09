#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "galaxy.h"
#include "constants.h"

#define N 50
#define W 540.0f
// TAU is 2*PI. We map 0..TAU to 0..255
#define TAU 6.28318530718f

// TAU is 2*PI. We map 0..TAU to 0..255
#define TAU 6.28318530718f


#define SCREEN_W 320
#define SCREEN_H 180


// Simulation state
static int16_t t, x, y;
static int cur_i, cur_j;
static bool initialized = false;

// Batch processing controls
#define PARTICLE_BATCH_SIZE 600
#define DECAY_BATCH_SIZE 2500

// Sine Lookup Table (0..255 representing 0..2PI)
// Values -127 to 127
static const int8_t sin_table[256] = {
    0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 59, 62, 65, 67, 70, 73, 75, 78, 80, 82, 85, 87, 89, 91, 94, 96, 98, 100, 102, 103, 105, 107, 108, 110, 112, 113, 114, 116, 117, 118, 119, 120, 121, 122, 123, 123, 124, 125, 125, 126, 126, 126, 126, 126, 127, 126, 126, 126, 126, 126, 125, 125, 124, 123, 123, 122, 121, 120, 119, 118, 117, 116, 114, 113, 112, 110, 108, 107, 105, 103, 102, 100, 98, 96, 94, 91, 89, 87, 85, 82, 80, 78, 75, 73, 70, 67, 65, 62, 59, 57, 54, 51, 48, 45, 42, 39, 36, 33, 30, 27, 24, 21, 18, 15, 12, 9, 6, 3, 0, -3, -6, -9, -12, -15, -18, -21, -24, -27, -30, -33, -36, -39, -42, -45, -48, -51, -54, -57, -59, -62, -65, -67, -70, -73, -75, -78, -80, -82, -85, -87, -89, -91, -94, -96, -98, -100, -102, -103, -105, -107, -108, -110, -112, -113, -114, -116, -117, -118, -119, -120, -121, -122, -123, -123, -124, -125, -125, -126, -126, -126, -126, -126, -127, -126, -126, -126, -126, -126, -125, -125, -124, -123, -123, -122, -121, -120, -119, -118, -117, -116, -114, -113, -112, -110, -108, -107, -105, -103, -102, -100, -98, -96, -94, -91, -89, -87, -85, -82, -80, -78, -75, -73, -70, -67, -65, -62, -59, -57, -54, -51, -48, -45, -42, -39, -36, -33, -30, -27, -24, -21, -18, -15, -12, -9, -6, -3
};

// Fixed Point Math: Q8.8
// 1.0 = 256
// Angle conversion: 1 radian = 40.74 units (0-255 scaling)
#define FP_SCALE 256
#define ONE_RAD_Q8 41 

// Helper: Get sine from table (input is Q8.8 value but treated as index)
// We need to convert the simulation value (Q8.8) to an angle index (0-255).
// In float: index = rad * 40.74.
// In Q8.8: val has scale 256. 1.0 is 256.
// We want 1.0 radian -> index 41.
// So: index = (val * 41) >> 8.
static int16_t get_sin_q88(int16_t angle_val) {
    // Convert Q8.8 value to 0-255 index
    // Using simple multiply and shift
    // Need int32 for multiply to be safe? 16-bit * 16-bit fit in 32.
    // Optimization: The input 'angle_val' is roughly -3.0 to 3.0 -> -768 to 768.
    // 768 * 41 = 31488. Fits in int16_t!
    // So we can use 16-bit multiply.
    
    int16_t idx_scaled = angle_val * 41;
    uint8_t idx = (uint8_t)(idx_scaled >> 8);
    
    // Table returns -127 to 127.
    // We want Q8.8 result (-256 to 256 for -1.0 to 1.0).
    // Shift left by 1 gives -254 to 254. Close enough.
    return (int16_t)sin_table[idx] << 1;
}

static int16_t get_cos_q88(int16_t angle_val) {
    int16_t idx_scaled = angle_val * 41;
    uint8_t idx = (uint8_t)((idx_scaled >> 8) + 64);
    return (int16_t)sin_table[idx] << 1;
}

// Forward declare plot
static void plot(int px, int py, uint8_t color_add);

void galaxy_init(void) {
    // 1. Initialize Palette
    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 256; i++) {
        uint16_t color = 0;
        if (i < 64) { // Fade to Blue
            int b = i/2 + 10; if(b>31) b=31;
            color = b; 
             RIA.rw0 = color & 0xFF;
             RIA.rw0 = color >> 8;
        } else if (i < 85) { // Blueish
            int r = 0; int g = i/3; int b = i/2 + 10;
            if (g>31) g=31; if (b>31) b=31;
            color = (r << 10) | (g << 5) | b;
            RIA.rw0 = color & 0xFF;
            RIA.rw0 = color >> 8;
        } else if (i < 170) { // Cyanish
            int r = (i-85)/2; int g = 31; int b = 31;
            if (r>31) r=31;
            color = (r << 10) | (g << 5) | b;
            RIA.rw0 = color & 0xFF;
            RIA.rw0 = color >> 8;
        } else { // Whitish
            int r = 31; int g = 31; int b = 31;
            color = (r << 10) | (g << 5) | b;
            RIA.rw0 = color & 0xFF;
            RIA.rw0 = color >> 8;
        }
    }

    // 2. Clear Simulation State
    t = 0; x = 0; y = 0;
    cur_i = 0; cur_j = 0;
    
    // Clear Screen (XRAM)
    RIA.addr0 = PIXEL_DATA_ADDR;
    RIA.step0 = 1;
    for (uint32_t k = 0; k < BITMAP_SIZE; k++) {
        RIA.rw0 = 0; 
    }
    
    // 3. Warmup: Draw first full frame
    // This loops through all N*N particles once to populate the screen
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
             // Logic equivalent to update loop
             int16_t i_FP = i << 8; // i as Q8.8
             int16_t r_i_FP = (i * 5) << 6; // r*i roughly. 
             // r = 0.125. i*0.125 = i/8. 
             // in Q8.8: i * (256/8) = i * 32.
             // Python 'r' is TAU/N = 6.28/50 = 0.1256.
             // 0.1256 * 256 = 32.15.
             // So i * 32 is a good approx.
             r_i_FP = i * 32;

             int16_t u = get_sin_q88(i_FP + y) + get_sin_q88(r_i_FP + x);
             int16_t v = get_cos_q88(i_FP + y) + get_cos_q88(r_i_FP + x);
             
             x = u + t;
             y = v;
             
             int px = ((u * 40) >> 8) + 160;
             int py = ((y * 40) >> 8) + 90;
             plot(px, py, 16);
        }
    }
    
    initialized = true;
}

// Helper to put pixel with additive blending
static void plot(int px, int py, uint8_t color_add) {
    // Optimized bounds check
    if ((unsigned)px >= SCREEN_W || (unsigned)py >= SCREEN_H) return;
    
    uint16_t addr = PIXEL_DATA_ADDR + (uint16_t)py * SCREEN_W + (uint16_t)px;
    
    RIA.addr0 = addr;
    uint8_t current = RIA.rw0; // Read
    
    if (current < 255) {
        uint16_t val = current + color_add;
        if (val > 255) val = 255;
        RIA.addr0 = addr; 
        RIA.rw0 = (uint8_t)val;
    }
}

static void decay_screen(void) {
    // Optimized Decay using Dual Port XRAM Access
    static uint16_t decay_ptr = 0;
    uint16_t addr = PIXEL_DATA_ADDR + decay_ptr;
    
    RIA.addr0 = addr;
    RIA.step0 = 1;
    RIA.addr1 = addr;
    RIA.step1 = 1;
    
    for (int k = 0; k < DECAY_BATCH_SIZE; k++) {
        uint8_t val = RIA.rw0;
        if (val > 0) {
            val--;
        }
        RIA.rw1 = val;
    }
    
    decay_ptr += DECAY_BATCH_SIZE;
    if (decay_ptr >= BITMAP_SIZE) decay_ptr = 0;
}

void galaxy_update(void) {
    if (!initialized) return;

    // 1. Process Particles
    for (int k = 0; k < PARTICLE_BATCH_SIZE; k++) {
        int16_t i_FP = cur_i << 8;
        int16_t r_i_FP = cur_i * 32; // approx r*i (0.125 * 256)
        
        int16_t u = get_sin_q88(i_FP + y) + get_sin_q88(r_i_FP + x);
        int16_t v = get_cos_q88(i_FP + y) + get_cos_q88(r_i_FP + x);
        
        x = u + t;
        y = v;
        
        int px = ((u * 40) >> 8) + 160;
        int py = ((y * 40) >> 8) + 90;
        
        plot(px, py, 16); 
        
        cur_j++;
        if (cur_j >= N) {
            cur_j = 0;
            cur_i++;
            if (cur_i >= N) {
                cur_i = 0;
                t += 25; // 0.1 * 256 = 25.6 -> 25
            }
        }
    }
    
    // 2. Decay Screen
    decay_screen();
}
