#ifndef FIXEDMATH_H
#define FIXEDMATH_H

#include <stdint.h>

// Fixed-point format: 16.16
typedef int32_t fixed_t;

#define INT_TO_FIXED(x) ((fixed_t)(x) << 16)
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * 65536.0))
#define FIXED_TO_INT(x) ((int16_t)((x) >> 16))

// Trig with 16-bit binary angle (0..65535 = 0..2PI)
fixed_t fast_sin(uint16_t angle);
fixed_t fast_cos(uint16_t angle);

// Constant for rad -> binary angle conversion (65536 / (2*PI))
#define RAD_TO_ANG_SCALE 10430L

#endif // FIXEDMATH_H
