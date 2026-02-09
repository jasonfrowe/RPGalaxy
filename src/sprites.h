#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool active;
    int16_t x, y;   // 12.4 Fixed Point (Coordinate << 4)
    int16_t vx, vy; // 12.4 Fixed Point Velocity
    uint8_t angle;  // Rotation (0-255)
    int16_t timer;
    uint8_t frame; // 0..3
} enemy_t;

#define MAX_ENEMIES 8

typedef struct {
    bool active;
    int16_t x, y;   // 12.4 Fixed Point
    int16_t vx, vy; // 12.4 Fixed Point
    uint8_t type;   // 0=Guardian (Cyan), 1=Gardener (Magenta)
    int16_t timer;
    uint8_t frame;
} worker_t;

#define MAX_WORKERS 8

extern enemy_t enemies[MAX_ENEMIES];
extern worker_t workers[MAX_WORKERS];
extern int16_t reticle_x, reticle_y;

void init_sprites(void);
void update_sprites(void); // Updates Reticle
void update_enemies(void);
void update_workers(void);
void spawn_enemy(int16_t x, int16_t y);
void spawn_worker(uint8_t type, int16_t x, int16_t y);
void update_reticle_position(int8_t dx, int8_t dy);

#endif // SPRITES_H
