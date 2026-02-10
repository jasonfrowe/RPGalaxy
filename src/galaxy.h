#ifndef GALAXY_H
#define GALAXY_H

void galaxy_init(void);
void galaxy_randomize(uint16_t seed);
void galaxy_infect(int16_t px, int16_t py);
void galaxy_heal(int16_t px, int16_t py);
#include <stdbool.h>

void galaxy_init(void);
void galaxy_init(void);
bool galaxy_tick(void); // Returns true when a full frame is completed
void galaxy_explosion(int16_t x, int16_t y, uint8_t type);

#endif // GALAXY_H
