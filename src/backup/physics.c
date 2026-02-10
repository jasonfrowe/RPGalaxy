#include <stdlib.h>
#include "physics.h"
#include "tables.h" /* For SIN_LUT */

/* 
   Parametric Elliptical Orbit Logic
   Strictly 8-bit / 16-bit math. No division, no 32-bit types.
   
   x = a * cos(angle)
   y = b * sin(angle)
   
   a = radius (semi-major axis)
   b = a * (1 - eccentricity)
   
   angle_io: 16-bit (8.8), integer part 0-255 used for LUT
*/

void update_parametric_orbit(int16_t *x_out, int16_t *y_out, uint16_t *angle_io, uint8_t radius, uint8_t eccentricity, uint8_t speed) {
    uint16_t ang_fixed = *angle_io;
    uint8_t ang_int = (ang_fixed >> 8) & 0xFF; // Upper byte
    
    // 1. Calculate semi-minor axis 'b'
    // b = radius * (256 - eccentricity) / 256
    // Max radius 85, max value 21760. Safe int16.
    
    uint16_t scale_b = 256 - eccentricity;
    // Cast to uint16_t to be explicit about desired width
    uint16_t b = ((uint16_t)radius * scale_b) >> 8;
    
    // 2. Lookup Sine/Cosine
    // SIN_LUT is 1.0 = 256. Range -256..256.
    int16_t c = SIN_LUT[(uint8_t)(ang_int + 64)];
    int16_t s = SIN_LUT[ang_int];
    
    // 3. Calculate Position relative to center
    // x = a * cos
    // y = b * sin
    // Max val: 85 * 256 = 21760. Safe int16.
    
    // Use uint16_t cast for absolute value math, then apply sign?
    // Actually, int16 * int16 -> int (16 bit signed on 6502).
    // 85 * 256 = 21760 (Positive).
    // 85 * -256 = -21760 (Negative).
    // Both fit in int16 range (-32768..32767).
    // So simple multiplication is SAFE.
    
    int16_t rel_x = ((int16_t)radius * c) >> 8;
    int16_t rel_y = ((int16_t)b * s) >> 8;
    
    // Output 12.4 fixed point (<< 4). Center 160, 90.
    *x_out = (160 + rel_x) << 4;
    *y_out = (90 + rel_y) << 4;
    
    // 4. Update Angle
    // Speed: 8.8 fixed point.
    // Kepler-lite: Faster when x is negative (left side)? Or just when "close"?
    // "Close" in parametric: r is distance.
    // r ~= radius when e=0. r < radius when e > 0 at certain angles.
    // Let's just modulate speed by cosine for simplicity?
    // speed_mod = speed + (speed * e * cos >> 8)?
    // Maximum modulation factor = 1.5x?
    // 150 * 128 * 1 (cos) >> 8 = 75. New speed 225.
    // 150 * 128 * -1 >> 8 = -75. New speed 75.
    // Range 75..225. Safe.
    
    int16_t modulation = ((int16_t)speed * (int16_t)eccentricity) >> 8; // Max 150 * 128 >> 8 = 75
    // Modulation * cos (1.0 = 256).
    // 75 * 256 >> 8 = 75.
    
    // We need to shift after multiplying by cos.
    // (modulation * c) >> 8.
    // 75 * 256 = 19200. Safe.
    int16_t delta_mod = (modulation * c) >> 8;
    
    // Ensure we don't stop or reverse
    int16_t new_speed = (int16_t)speed - delta_mod; // Subtract cos term (faster at -x?)
    if (new_speed < 20) new_speed = 20;

    *angle_io = ang_fixed + (uint16_t)new_speed;
}
