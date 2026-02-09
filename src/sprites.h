#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include <stdbool.h>

void init_sprites(void);
void update_sprites(void);
void update_reticle_position(int8_t dx, int8_t dy);

#endif // SPRITES_H
