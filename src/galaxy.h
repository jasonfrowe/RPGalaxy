#ifndef GALAXY_H
#define GALAXY_H

void galaxy_init(void);
void galaxy_randomize(uint16_t seed);
void galaxy_infect(int16_t px, int16_t py);
void galaxy_heal(int16_t px, int16_t py);
#include <stdbool.h>

void galaxy_init(void);
bool galaxy_tick(void); // Returns true when a full frame is completed

#endif // GALAXY_H
