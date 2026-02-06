#include <rp6502.h>
#include <stdbool.h>

#include "galaxy.h"
#include "graphics.h"
#include "constants.h"

// Parameters
// Original N=200 -> 40,000 pts. Too slow for 6502.
// Try smaller N initially. 
// N=125 -> 15,625 pts. (Uses ~47KB RAM).
#define GALAXY_N 125
// Update 400 dots per frame -> 39 frames for full update (~0.66s cycle)
#define BATCH_SIZE 25

// Stores valid coordinates (0..SCREEN_WIDTH/HEIGHT) or a marker for invalid/off-screen.
// Using uint16_t for X allows valid range 0-320. 
// Using uint8_t for Y allows valid range 0-180.
// X=0xFFFF is "not drawn" marker.
static uint16_t last_x[GALAXY_N * GALAXY_N];
static uint8_t  last_y[GALAXY_N * GALAXY_N];
static bool first_frame = true;

// Pre-scaled LUTs: sin(angle) * 960
static const int16_t sin_lut[256] = {
0,23,47,70,94,117,140,164,187,210,233,256,278,301,323,345,367,389,410,431,452,473,493,513,533,552,571,590,609,627,644,661,678,695,711,726,742,756,771,784,798,811,823,835,846,857,867,877,886,895,903,911,918,925,931,936,941,945,949,952,955,957,958,959,960,959,958,957,955,952,949,945,941,936,931,925,918,911,903,895,886,877,867,857,846,835,823,811,798,784,771,756,742,726,711,695,678,661,644,627,609,590,571,552,533,513,493,473,452,431,410,389,367,345,323,301,278,256,233,210,187,164,140,117,94,70,47,23,0,-23,-47,-70,-94,-117,-140,-164,-187,-210,-233,-256,-278,-301,-323,-345,-367,-389,-410,-431,-452,-473,-493,-513,-533,-552,-571,-590,-609,-627,-644,-661,-678,-695,-711,-726,-742,-756,-771,-784,-798,-811,-823,-835,-846,-857,-867,-877,-886,-895,-903,-911,-918,-925,-931,-936,-941,-945,-949,-952,-955,-957,-958,-959,-960,-959,-958,-957,-955,-952,-949,-945,-941,-936,-931,-925,-918,-911,-903,-895,-886,-877,-867,-857,-846,-835,-823,-811,-798,-784,-771,-756,-742,-726,-711,-695,-678,-661,-644,-627,-609,-590,-571,-552,-533,-513,-493,-473,-452,-431,-410,-389,-367,-345,-323,-301,-278,-256,-233,-210,-187,-164,-140,-117,-94,-70,-47,-23
};
static const int16_t cos_lut[256] = {
960,959,958,957,955,952,949,945,941,936,931,925,918,911,903,895,886,877,867,857,846,835,823,811,798,784,771,756,742,726,711,695,678,661,644,627,609,590,571,552,533,513,493,473,452,431,410,389,367,345,323,301,278,256,233,210,187,164,140,117,94,70,47,23,0,-23,-47,-70,-94,-117,-140,-164,-187,-210,-233,-256,-278,-301,-323,-345,-367,-389,-410,-431,-452,-473,-493,-513,-533,-552,-571,-590,-609,-627,-644,-661,-678,-695,-711,-726,-742,-756,-771,-784,-798,-811,-823,-835,-846,-857,-867,-877,-886,-895,-903,-911,-918,-925,-931,-936,-941,-945,-949,-952,-955,-957,-958,-959,-960,-959,-958,-957,-955,-952,-949,-945,-941,-936,-931,-925,-918,-911,-903,-895,-886,-877,-867,-857,-846,-835,-823,-811,-798,-784,-771,-756,-742,-726,-711,-695,-678,-661,-644,-627,-609,-590,-571,-552,-533,-513,-493,-473,-452,-431,-410,-389,-367,-345,-323,-301,-278,-256,-233,-210,-187,-164,-140,-117,-94,-70,-47,-23,0,23,47,70,94,117,140,164,187,210,233,256,278,301,323,345,367,389,410,431,452,473,493,513,533,552,571,590,609,627,644,661,678,695,711,726,742,756,771,784,798,811,823,835,846,857,867,877,886,895,903,911,918,925,931,936,941,945,949,952,955,957,958,959
};

// Global state using 8.8 fixed point for accumulation
static int16_t t_fp = 0; 
static int16_t gx_fp = 0;
static int16_t gy_fp = 0;

// Render loop state
static int cur_i = 0;
static int cur_j = 0;

static inline int16_t lut_sin(uint8_t idx) {
    return sin_lut[idx];
}

static inline int16_t lut_cos(uint8_t idx) {
    return cos_lut[idx];
}

void galaxy_init(void) {
    t_fp = 0;
    gx_fp = 0;
    gy_fp = 0;
    cur_i = 0;
    cur_j = 0;
}

// Python logic:
// r = TAU / N
// for i in range(N):
//    for j in range(N):
//       u = sin(i + y) + sin(r * i + x)
//       v = cos(i + y) + cos(r * i + x)
//       x = u + t
//       y = v
//       fill(i, c, 99) -> color depends on i, j? python says `c` which is `j`.
//       circle(...)

// Optimization:
// r_fp = (256 / N) if we map 2PI to 256. 
// Or better: angle units = 256 per rotation.
// r_fixed = 256 / N

void galaxy_draw(void) {
    if (first_frame) {
        // Clear screen once
        RIA.addr0 = 0;
        RIA.step0 = 1;
        for (unsigned i = 0; i < BITMAP_SIZE; i++) {
            RIA.rw0 = 0;
        }
        // Init cache
        for (int k = 0; k < GALAXY_N * GALAXY_N; k++) {
            last_x[k] = 0xFFFF;
        }
        first_frame = false;
    }

    int16_t u, v;

    // Calc base pointer offset
    uint16_t offset = (cur_i * GALAXY_N) + cur_j;
    uint16_t *lx = &last_x[offset];
    uint8_t  *ly = &last_y[offset];

    // Direct RIA access prevents function call overhead
    // RIA.step0 should be 1.
    RIA.step0 = 1;

    // Process a batch
    for (int b = 0; b < BATCH_SIZE; b++) {
        // Hoist i-dependent math (recalc if cur_j resets)
        // Since we loop j inside, we can just use cur_i/cur_j vars
        
        // Precise angle calc to ensure full coverage 0-255
        uint8_t angle_ri = (uint8_t)(((uint16_t)cur_i * 256) / GALAXY_N); 
        uint8_t i_ang = (uint8_t)(cur_i * 41);
        uint8_t i_col_base = 16 + (cur_i * 11);

        // 1. ERASE OLD PIXEL
        uint16_t old_x = *lx;
        if (old_x != 0xFFFF) {
            uint8_t old_y = *ly;
            RIA.addr0 = (uint16_t)(old_x + (SCREEN_WIDTH * old_y));
            RIA.rw0 = 0; // Black
        }

        // 2. CALC
        // y contribution
        // Combined: ((gy_int * 41) + ((gy_frac * 41) >> 8))
        // But simplify: just use map 256 -> 256? No python code maps radians.
        // Let's stick to the 41 scaling for angle correctness.
        // gy_fp and gx_fp are 8.8.
        uint8_t y_ang = (uint8_t)(((gy_fp >> 8) * 41) + ((gy_fp & 0xFF) * 41 >> 8));
        uint8_t angle1 = i_ang + y_ang;

        // x contribution
        uint8_t x_ang = (uint8_t)(((gx_fp >> 8) * 41) + ((gx_fp & 0xFF) * 41 >> 8));
        uint8_t angle2 = angle_ri + x_ang;

        int16_t s1 = lut_sin(angle1);
        int16_t s2 = lut_sin(angle2);
        int16_t c1 = lut_cos(angle1);
        int16_t c2 = lut_cos(angle2);

        u = s1 + s2; 
        v = c1 + c2;

        // Update state 
        // u is (Scale 960).
        // gx_fp needs (Scale 240) approx 8.8.
        // 960 >> 2 = 240.
        // Feedback loop has 4x more precision than before (Scale 240 vs 60).
        gx_fp = (u >> 2) + t_fp;
        gy_fp = (v >> 2);

        // Screen coords
        // u is Scale 960. Screen is Scale 60.
        // 960 >> 4 = 60.
        int16_t sx = 160 + (u >> 4);
        int16_t sy_raw = (v >> 4);
        int16_t sy = 90 + sy_raw - (sy_raw >> 2);

        // 3. DRAW NEW PIXEL & STORE
        if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
            // Optimized rainbow color: No modulo
            uint8_t col = i_col_base + cur_j;

            RIA.addr0 = (uint16_t)(sx + (SCREEN_WIDTH * sy));
            RIA.rw0 = col;

            *lx = (uint16_t)sx;
            *ly = (uint8_t)sy;
        } else {
            *lx = 0xFFFF;
        }

        lx++;
        ly++;
        cur_j++;
        if (cur_j >= GALAXY_N) {
            cur_j = 0;
            cur_i++;
            if (cur_i >= GALAXY_N) {
                cur_i = 0;
                // Cycle Complete
                
                // Update Time Step Here (Once per full cycle)
                // Slow evolution: 1 unit per cycle.
                t_fp += 1; 

                // Reset pointers
                lx = last_x;
                ly = last_y;
            }
        }
    }
}
