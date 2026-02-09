#include <rp6502.h>
#include <stdint.h>
#include <stdlib.h> // for abs
#include "galaxy.h"
#include "tables.h"
#include "constants.h"
#include "graphics.h"
#include "sprites.h" // For enemies/workers

// #define N 64 (Replaced by N 80 below)
#define SCALE 60 // Screen scale factor
#define T_INC 5 // 0.2 in 8.8 fixed point

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
    
    // Hubble Palette (Teal & Gold)
    
    // Channel 1 (Pink var): Gold/Hydrogen (Rust -> Gold -> Pale Yellow)
    const uint8_t P_R[] = {20, 40, 60, 80, 100, 130, 160, 190, 210, 225, 235, 245, 250, 252, 255, 255};
    const uint8_t P_G[] = {5,  10, 20, 30,  45,  60,  80, 100, 120, 140, 160, 180, 200, 220, 240, 255};
    const uint8_t P_B[] = {0,   0,  0,  5,  10,  15,  25,  35,  50,  65,  85, 105, 130, 160, 190, 220};

    // Channel 2 (Cyan var): Azure/Oxygen (Deep Blue -> Teal -> Ice Blue)
    const uint8_t C_R[] = {0,   0,  0,  0,   5,  10,  20,  30,  45,  60,  80, 100, 130, 160, 190, 220};
    const uint8_t C_G[] = {5,  15, 30, 50,  70,  90, 110, 130, 150, 170, 190, 210, 225, 235, 245, 255};
    const uint8_t C_B[] = {20, 40, 60, 80, 100, 125, 150, 175, 200, 215, 225, 235, 245, 250, 252, 255};

    for (int i = 0; i < 256; i++) {
        uint8_t pink = (i >> 4) & 0x0F;
        uint8_t cyan = i & 0x0F;
        
        // Additive Mixing
        uint16_t r_sum = P_R[pink] + C_R[cyan];
        uint16_t g_sum = P_G[pink] + C_G[cyan];
        uint16_t b_sum = P_B[pink] + C_B[cyan];
        
        uint8_t r = (r_sum > 255) ? 255 : (uint8_t)r_sum;
        uint8_t g = (g_sum > 255) ? 255 : (uint8_t)g_sum;
        uint8_t b = (b_sum > 255) ? 255 : (uint8_t)b_sum;
        
        // Convert to RP6502 format with Alpha set
        // Index 0 must be transparent for Sprites
        uint16_t color = COLOR_FROM_RGB8(r, g, b);
        if (i > 0) color |= COLOR_ALPHA_MASK;
        
        // Write Little Endian
        RIA.rw0 = color & 0xFF;
        RIA.rw0 = (color >> 8) & 0xFF;
    }
}



#define N 80 // Increased density

// Particle State: 0 = Normal, 1 = Infected (Cyan Only)
static uint8_t particle_state[N];

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
    
    // Initialize particles to Normal
    for (int i = 0; i < N; i++) {
        particle_state[i] = 0;
    }
}

void galaxy_randomize(uint16_t seed)
{
    // Simple LCG
    x = seed;
    y = seed * 3 + 12345;
    t = seed * 7 + 54321;
}

// Stride for decay to save CPU cycles
// Decay 1/2 of the screen per frame in a rolling pattern
static uint8_t decay_pass = 0;


    // State Machine Variables
typedef enum {
    STATE_DECAY,
    STATE_TIME,
    STATE_PARTICLES
} galaxy_state_t;

static galaxy_state_t g_state = STATE_DECAY;
static uint16_t decay_idx = 0;
static uint8_t part_i = 0;
static uint8_t part_j = 0;

// Cache for outer loop values to avoid recomputing
static uint8_t cached_ri_idx;
static uint8_t cached_i_rad_idx;

bool galaxy_tick(void)
{
    // Return true if frame completed
    // Return false if work still pending for this frame
    
    switch (g_state) {
        case STATE_DECAY:
             // Process 256 pixels per tick
             // Total 28800 pixels / 256 = 112 ticks
            {
                uint16_t end_idx = decay_idx + 256;
                if (end_idx > (BITMAP_SIZE / 2)) end_idx = (BITMAP_SIZE / 2);
                
                // Set up RIA for this batch
                // Start index + offset for current pass
                uint16_t current_addr = PIXEL_DATA_ADDR + decay_pass + (decay_idx * 2);
                
                RIA.addr0 = current_addr;
                RIA.step0 = 2; 
                
                for (uint16_t k = decay_idx; k < end_idx; k++) {
                    uint8_t val = RIA.rw0;
                    if (val > 0) {
                        uint8_t pink = (val >> 4) & 0x0F;
                        uint8_t cyan = val & 0x0F;
                        
                        // Exponential Decay (Divide by 2)
                        // 15->7->3->1->0. Kills blobs fast.
                        pink >>= 1;
                        cyan >>= 1;
                        
                        val = (pink << 4) | cyan;
                        RIA.addr0 -= 2; 
                        RIA.rw0 = val;
                    }
                }
                
                decay_idx = end_idx;
                
                if (decay_idx >= (BITMAP_SIZE / 2)) {
                    // Decay Done
                    decay_idx = 0;
                    decay_pass = (decay_pass + 1) & 1;
                    g_state = STATE_TIME;
                }
            }
            return false;
            
        case STATE_TIME:
            // Update time once per frame
            if (t > 1608) t -= 1608; 
            t += T_INC;
            
            // Prepare for particles
            part_i = 0;
            part_j = 0;
            
            // Precompute first outer loop values
            cached_ri_idx = (0 * 4);
            cached_i_rad_idx = 0;
            
            g_state = STATE_PARTICLES;
            return false;
            
        case STATE_PARTICLES:
            // Process 8 interactions per tick
            // 6400 total / 8 = 800 ticks
            // Total ticks per frame = 112 + 1 + 800 = ~913.
            // At 60Hz audio updates, we can do 1 tick per audio poll?
            // If music is 60hz, we have 16ms. 
            // 8 interactions * 9 pixels * overhead might be 1-2ms. Safe.
            
            for (int k = 0; k < 8; k++) {
                // If j wraps, increment i
                if (part_j >= N) {
                    part_j = 0;
                    part_i++;
                    
                    if (part_i >= N) {
                        // All particles done
                        g_state = STATE_DECAY;
                        return true; // Frame Completed
                    }
                    
                    // Precompute new outer loop values
                    cached_ri_idx = (part_i * 4);
                    cached_i_rad_idx = (uint8_t)(((int32_t)part_i * RAD_SCALE));
                }
                
                // Process interaction (part_i, part_j)
                uint8_t i = part_i;
                uint8_t j = part_j;
                
                // Calculation
                uint8_t y_idx = (uint8_t)(((int32_t)y * RAD_SCALE) >> 8);
                uint8_t x_idx = (uint8_t)(((int32_t)x * RAD_SCALE) >> 8);
                
                uint8_t idx_u1 = cached_i_rad_idx + y_idx;
                uint8_t idx_u2 = cached_ri_idx + x_idx;
                
                int16_t u = SIN_LUT[idx_u1] + SIN_LUT[idx_u2];
                int16_t v = SIN_LUT[(uint8_t)(idx_u1 + 64)] + SIN_LUT[(uint8_t)(idx_u2 + 64)];
                
                // Update Globals for next step
                x = u + t;
                y = v; 
                
                int16_t screen_x = ((u * SCALE) >> 8) + (SCREEN_WIDTH / 2);
                int16_t screen_y = ((v * SCALE) >> 8) + (SCREEN_HEIGHT / 2);

                // --- INFECTION / HEALING CHECK ---
                // Only check for i (the particle being updated)
                // Check once per inner loop? No, that's expensive (N*N).
                // But we need to check the *current* screen_x/y which changes every step.
                // So yes, we check every step.
                
                // Check Enemies (Infect)
                for (int e = 0; e < MAX_ENEMIES; e++) {
                    if (enemies[e].active) {
                        int16_t ex = (enemies[e].x >> 4) + 8; // Center
                        int16_t ey = (enemies[e].y >> 4) + 8;
                        

                        
                        // RADIUS 16 for testing
                        if (abs(screen_x - ex) < 16 && abs(screen_y - ey) < 16) {
                            particle_state[i] = 1; // Infected
                        }
                    }
                }
                
                // Check Gardeners (Heal)
                for (int w = 0; w < MAX_WORKERS; w++) {
                    if (workers[w].active && workers[w].type == 1) { // Type 1 = Gardener
                        int16_t wx = (workers[w].x >> 4) + 8;
                        int16_t wy = (workers[w].y >> 4) + 8;
                        if (abs(screen_x - wx) < 16 && abs(screen_y - wy) < 16) {
                            particle_state[i] = 0; // Healed
                        }
                    } 
                }
                
                uint8_t is_infected = particle_state[i];
                
                // Draw 3x3 Blur Patch
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int16_t px = screen_x + dx;
                        int16_t py = screen_y + dy;
                        
                        if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                            uint16_t addr = (uint16_t)px + (SCREEN_WIDTH * (uint16_t)py);
                            
                            // Color Logic
                            uint8_t is_pink = (i < (N/2));
                            if (is_infected) is_pink = 0; // Force Cyan
                            
                            RIA.addr0 = addr;
                            RIA.step0 = 0; 
                            uint8_t old_val = RIA.rw0;
                            
                            uint8_t pink = (old_val >> 4) & 0x0F;
                            uint8_t cyan = old_val & 0x0F;
                            
                            // Strong Accumulation (+6 Center, +2 Neighbor)
                            uint8_t add_amount = (dx == 0 && dy == 0) ? 6 : 2;
                            
                            if (is_pink) {
                                if (pink < (15 - add_amount)) pink += add_amount;
                                else pink = 15;
                            } else {
                                if (cyan < (15 - add_amount)) cyan += add_amount;
                                else cyan = 15;
                            }
                            
                            RIA.rw0 = (pink << 4) | cyan;
                        }
                    }
                }
                
                 part_j++;
            }
            return false;
    }
    return false;
}


