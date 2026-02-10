#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>

/*
 * update_parametric_orbit
 * Calculates position using parametric ellipse equations:
 * x = a * cos(t)
 * y = b * sin(t)
 * where b varies with eccentricity.
 * 
 * 16-bit safe. No division.
 */
void update_parametric_orbit(int16_t *x_out, int16_t *y_out, uint16_t *angle_io, uint8_t radius, uint8_t eccentricity, uint8_t speed);

#endif
