#include <stdlib.h>
#include "physics.h"
#include "tables.h" /* For SIN_LUT */

/* 
   Geometric (Parametric) Orbit Logic
   Strictly 8-bit / 16-bit math. 
   
   x = a * cos(angle)
   y = b * sin(angle)
   
   a = radius (semi-major axis)
   b = a * (1 - eccentricity)
   
   angle_io: 16-bit (8.8), integer part 0-255 used for LUT
*/

// Helper for safe 8.8 fixed point multiplication of signed values
// Returns (a * b) >> 8
static int16_t safe_mul_shift(int16_t a, int16_t b) {
    int16_t res;
    int16_t sign = 1;
    if (a < 0) { a = -a; sign = -sign; }
    if (b < 0) { b = -b; sign = -sign; }
    
    // Low-overhead check:
    // With max radius 85 and max cos 256, product is 21760.
    // Fits in uint16 comfortably.
    
    uint16_t prod = ((uint16_t)a * (uint16_t)b) >> 8;
    
    res = (int16_t)prod;
    if (sign < 0) res = -res;
    return res;
}

// Calculates 8-bit angle (0-255) from vector (x,y)
// Maps 0..360 degrees to 0..255
uint8_t vector_to_angle(int16_t x, int16_t y) {
    if (x == 0 && y == 0) return 0;
    
    // Determine Octant
    int16_t abs_x = (x < 0) ? -x : x;
    int16_t abs_y = (y < 0) ? -y : y;
    
    uint8_t angle = 0;
    
    // 1. Reduce to First Octant (0..45 degrees)
    // We want ratio = min / max.
    // Result 0..32 (approx 45 degrees in 256-scale).
    
    int16_t max_val = (abs_x > abs_y) ? abs_x : abs_y;
    int16_t min_val = (abs_x > abs_y) ? abs_y : abs_x;
    
    // Linear approximation: angle = (min * 32) / max
    // Max error ~4 degrees (at 22.5 deg). Good enough for spawn.
    uint8_t base_angle = 0;
    if (max_val > 0) {
        base_angle = (uint8_t)(( (int32_t)min_val * 32 ) / max_val);
    }
    
    // 2. Map back to correct Octant
    if (abs_x >= abs_y) {
        // Octant 0, 3, 4, 7 (X dominant)
        if (x >= 0) {
            // Right side
            if (y >= 0) angle = base_angle;         // Octant 0 (0..32)
            else        angle = 256 - base_angle;   // Octant 7 (224..255)
        } else {
            // Left side
            if (y >= 0) angle = 128 - base_angle;   // Octant 3 (96..128)
            else        angle = 128 + base_angle;   // Octant 4 (128..160)
        }
    } else {
        // Octant 1, 2, 5, 6 (Y dominant)
        // Base angle calculates offset from Y axis (90 or 270)
        // Since base_angle is min/max, it's measuring 'co-angle'.
        // So we want (90 - angle) magnitude? 
        // No, min/max gives angle from nearest axis.
        // For (1, 10). min=1, max=10. angle=3.
        // Correct is 87 deg (near 90) or 93.
        // So we subtract/add base_angle from 64 (90 deg) or 192 (270 deg).
        
        if (y >= 0) {
            // Top (y positive is down? No, usually y down in 2D gfx)
            // Wait, standard Cartesian: Y up. Screen: Y down.
            // Let's assume standard angles: 0=Right, 64=Down, 128=Left, 192=Up.
            // If y > 0 (Down).
            if (x >= 0) angle = 64 - base_angle;    // Octant 1
            else        angle = 64 + base_angle;    // Octant 2
        } else {
            // Bottom (Up)
            if (x >= 0) angle = 192 + base_angle;   // Octant 6
            else        angle = 192 - base_angle;   // Octant 5
        }
    }
    
    return angle;
}

void update_geometric_orbit(int16_t *x_out, int16_t *y_out, uint16_t *angle_io, uint8_t radius, uint8_t eccentricity, uint8_t speed) {
    uint16_t ang_fixed = *angle_io;
    uint8_t ang_int = (ang_fixed >> 8) & 0xFF; 
    
    // 1. Semi-minor axis b
    // b = radius * (1 - e)
    // Both positive. Safe shift.
    uint16_t scale_b = 256 - eccentricity;
    uint16_t b = ((uint16_t)radius * scale_b) >> 8;
    
    // 2. Lookup Sine/Cosine
    int16_t c = SIN_LUT[(uint8_t)(ang_int + 64)];
    int16_t s = SIN_LUT[ang_int];
    
    // 3. Calculate Position
    // x = radius * c
    // y = b * s
    // Use safe_mul_shift to handle sign of c/s.
    
    int16_t rel_x = safe_mul_shift((int16_t)radius, c);
    int16_t rel_y = safe_mul_shift((int16_t)b, s);
    
    // Keplerian Offset: Shift Center so Focus is at (0,0).
    // Center of Ellipse is at (-ae, 0) relative to Focus.
    // So X_focus = X_ellipse + ae? 
    // Std Kepler: r = a(1-e) at Pericenter (Right).
    // X_ellipse at t=0 is +a.
    // If we want +a to map to a(1-e), we subtract ae.
    // X_focus = X_ellipse - ae.
    
    int16_t offset = ((uint16_t)radius * (uint16_t)eccentricity) >> 8;
    rel_x -= offset;
    
    *x_out = (160 + rel_x) << 4;
    *y_out = (90 + rel_y) << 4;
    
    // 4. Update Angle (Kepler-lite)
    // modulation = speed * eccentricity. Both pos.
    uint16_t mod_factor = ((uint16_t)speed * (uint16_t)eccentricity) >> 8;
    
    // delta = mod_factor * c. Signed.
    int16_t delta_mod = safe_mul_shift((int16_t)mod_factor, c);
    
    int16_t new_speed = (int16_t)speed - delta_mod; 
    if (new_speed < 20) new_speed = 20;

    *angle_io = ang_fixed + (uint16_t)new_speed;
}
