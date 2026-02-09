#include <rp6502.h>
#include <stdint.h>
#include <math.h> 
#include <stdlib.h>
#include <stdbool.h>

// #include "galaxy.h" // Removed as logic moved to galaxy.c checks
#include "sprites.h"
#include "tables.h"
#include "constants.h"

#define SPRITE_CONFIG_ADDR 0xFD00
#define SPRITE_DATA_ADDR   0xF500 // Reticle Data (Moved to 0xF500)

// Top of file helpers
// Squared distance check for collision (16x16 sprites, radius ~8)
// Threshold: (8+8)^2 = 256. Let's use 16*16 = 256.
bool check_collision(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int16_t dx = (x1 - x2) >> 4;
    int16_t dy = (y1 - y2) >> 4;
    return ((dx*dx + dy*dy) < 200); // Slightly forgiving
}

static uint16_t sprite_angle = 0; // 0..255
static uint16_t pulse_time = 0;
int16_t reticle_x = 144; // Start center
int16_t reticle_y = 74;

// Enemy & Worker Data (Shifted to 0xE500 base)
#define ENEMY_DATA_ADDR    0xE500
#define WORKER_DATA_ADDR   0xED00

// Config Structs (Moved to 0xE310 to follow Bitmap Config)
// Affine Sprite Stride is 20 bytes! 8 * 20 = 160 (0xA0)
#define ENEMY_CONFIG_BASE  0xE310 
#define WORKER_CONFIG_BASE 0xE3B0 // 0xE310 + 0xA0 = 0xE3B0       

enemy_t enemies[MAX_ENEMIES];
worker_t workers[MAX_WORKERS];

void spawn_worker(uint8_t type, int16_t x, int16_t y) {
    for (int i = 0; i < MAX_WORKERS; i++) {
        if (!workers[i].active) {
            workers[i].active = true;
            workers[i].type = type;
            workers[i].x = x << 4; // 12.4
            workers[i].y = y << 4;
            
            // Launch Velocity based on Reticle Angle
            // Angle is 0..255.
            uint8_t angle = (uint8_t)sprite_angle;
            
            // Speed: Fixed for now, or based on pulse?
            // Let's use a good orbital injection speed.
            int16_t speed = 48; // 3.0 pixels/frame
            
            // vx = speed * cos(angle)
            // vy = speed * sin(angle)
            // SIN_LUT is 8.8 (256 = 1.0)
            
            int32_t c = (int32_t)SIN_LUT[(uint8_t)(angle + 64)];
            int32_t s = (int32_t)SIN_LUT[angle];
            
            workers[i].vx = (int16_t)((c * speed) >> 8);
            workers[i].vy = (int16_t)((s * speed) >> 8);

            workers[i].frame = (type == 0) ? 0 : 2; // Start frame
            workers[i].timer = 0;
            return;
        }
    }
}

void update_workers(void) {
    int16_t cx = 160 << 4;
    int16_t cy = 90 << 4;

    for (int i = 0; i < MAX_WORKERS; i++) {
        unsigned config_addr = WORKER_CONFIG_BASE + (i * sizeof(vga_mode4_asprite_t));
        
        if (!workers[i].active) {
            xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, -32);
            continue;
        }

        // --- GRAVITY PHYSICS ---
        
        // Distance to center
        int16_t dx = cx - workers[i].x;
        int16_t dy = cy - workers[i].y;
        
        // Gravity Force (Proportional to distance = Spring, Constant = Real Gravity?)
        // Let's use Spring-like for stability first (Hooks Law F=kx)
        // Works well for keeping things on screen.
        int16_t ax = dx / 128; // Tuning
        int16_t ay = dy / 128;
        
        workers[i].vx += ax;
        workers[i].vy += ay;
        
        // Drag (Air Resistance) -> Prevents infinite speed
        workers[i].vx -= (workers[i].vx / 128);
        workers[i].vy -= (workers[i].vy / 128);

        // Move
        workers[i].x += workers[i].vx;
        workers[i].y += workers[i].vy;
        
        // Kill if way off screen? Or Wrap?
        // Let gravity pull them back.
        
        // Screen Coords
        int16_t px = workers[i].x >> 4;
        int16_t py = workers[i].y >> 4;
        
        // --- INTERACTION LOGIC ---
        if (workers[i].type == 0) {
            // GUARDIAN (Cyan): Seek & Destroy Enemies
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (enemies[e].active) {
                    if (check_collision(workers[i].x, workers[i].y, enemies[e].x, enemies[e].y)) {
                        // Collision! Destroy Enemy
                        enemies[e].active = false;
                        // Destroy Worker too? Or keep going?
                        // Let's keep guardian alive for now, or reduce timer?
                        // Let's add a "Hit" visual? Maybe later.
                    }
                }
            }
        } else {
            // GARDENER (Magenta): Heal Galaxy
            // Heal pixels at current location
        }

        // Animation
        workers[i].timer++;
        if (workers[i].timer > 8) {
            workers[i].timer = 0;
            // Toggle Frame: Guardian (0-1), Gardener (2-3)
            uint8_t base = (workers[i].type == 0) ? 0 : 2;
            uint8_t offset = (workers[i].frame - base + 1) & 1;
            workers[i].frame = base + offset;
        }
        
        // Render (Affine)
        // Rotate workers to face velocity? Or spin? 
        // Let's just spin them slowly based on position for now
        int16_t rot = workers[i].x; 
        
        int16_t scale = 256; 
        int16_t c = SIN_LUT[(uint8_t)(rot + 64)]; 
        int16_t s = SIN_LUT[(uint8_t)rot];
        
        int16_t A = ((int32_t)scale * c) >> 8; 
        int16_t B = ((int32_t)scale * (-s)) >> 8;
        int16_t C = ((int32_t)scale * s) >> 8;
        int16_t D = ((int32_t)scale * c) >> 8;
        
        // Center 8,8
        int32_t center_fixed = 8 << 8;
        int16_t TX = (int16_t)(center_fixed - ((int32_t)A * 8) - ((int32_t)B * 8));
        int16_t TY = (int16_t)(center_fixed - ((int32_t)C * 8) - ((int32_t)D * 8));
        
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[0], A); 
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[1], B);   
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[2], TX);   
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[3], C);   
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[4], D); 
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[5], TY);   
        
        unsigned sprite_ptr = WORKER_DATA_ADDR + (workers[i].frame * 512);

        xram0_struct_set(config_addr, vga_mode4_asprite_t, x_pos_px, px);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, py);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, xram_sprite_ptr, sprite_ptr);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, log_size, 4); // 16x16
        xram0_struct_set(config_addr, vga_mode4_asprite_t, has_opacity_metadata, false);
    }
}

void spawn_enemy(int16_t x, int16_t y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].active = true;
            enemies[i].x = x << 4; // Convert to 12.4
            enemies[i].y = y << 4;
            
            // Initial Velocity: Push towards center slightly
             int16_t cx = 160 << 4;
            int16_t cy = 90 << 4;
            int16_t dx = cx - enemies[i].x;
            int16_t dy = cy - enemies[i].y;
            
            enemies[i].vx = dx / 256; 
            enemies[i].vy = dy / 256;
            
            enemies[i].angle = 0;
            enemies[i].frame = 0;
            enemies[i].timer = 0;
            return;
        }
    }
}

// Squared distance check for collision (16x16 sprites, radius ~8)
// Threshold: (8+8)^2 = 256. Let's use 16*16 = 256.


void update_enemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        unsigned config_addr = ENEMY_CONFIG_BASE + (i * sizeof(vga_mode4_asprite_t)); // NOW AFFINE STRUCT
        
        if (!enemies[i].active) {
            // Move offscreen
            xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, -32);
            continue;
        }

        // Logic: Seek Center (Spring Physics)
        // Target: Center (160, 90)
        int16_t target_x = 160 << 4;
        int16_t target_y = 90 << 4;
        
        int16_t dx = target_x - enemies[i].x;
        int16_t dy = target_y - enemies[i].y;
        
        // Acceleration proportional to distance (Spring 1/256)
        int16_t ax = dx / 256;
        int16_t ay = dy / 256;
        
        // Apply Acceleration
        enemies[i].vx += ax;
        enemies[i].vy += ay;
        
        // Apply Drag (Friction) to prevent eternal orbiting
        // v = v - (v/64)
        enemies[i].vx -= (enemies[i].vx / 64);
        enemies[i].vy -= (enemies[i].vy / 64);

        // Move
        enemies[i].x += enemies[i].vx;
        enemies[i].y += enemies[i].vy;
        

        
        // Logic: Rotate based on velocity? Or just spin?
        // Let's spin for now.
        enemies[i].angle += 2;

        // Animation
        enemies[i].timer++;
        if (enemies[i].timer > 8) { // Animation Speed
            enemies[i].timer = 0;
            enemies[i].frame = (enemies[i].frame + 1) & 3; // Cycle 0-3
        }
        
        // Render - Affine Calculation
        int16_t scale = 256; // 1.0
        uint8_t angle = enemies[i].angle;
        int16_t c = SIN_LUT[(uint8_t)(angle + 64)]; // cos
        int16_t s = SIN_LUT[angle]; // sin
        
        int16_t A = ((int32_t)scale * c) >> 8; 
        int16_t B = ((int32_t)scale * (-s)) >> 8;
        int16_t C = ((int32_t)scale * s) >> 8;
        int16_t D = ((int32_t)scale * c) >> 8;

        // Centering Logic (16x16)
        int16_t cx = 8; // Center of 16x16 is 8
        int16_t cy = 8;
        int32_t center_fixed = 8 << 8; // 2048
        
        // TX/TY
        int16_t TX = (int16_t)(center_fixed - ((int32_t)A * cx) - ((int32_t)B * cy));
        int16_t TY = (int16_t)(center_fixed - ((int32_t)C * cx) - ((int32_t)D * cy));

        unsigned sprite_ptr = ENEMY_DATA_ADDR + (enemies[i].frame * 512);

        // Update Affine Struct
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[0], A);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[1], B);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[2], TX);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[3], C); 
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[4], D); 
        xram0_struct_set(config_addr, vga_mode4_asprite_t, transform[5], TY);
        
        // CONVERT BACK TO PIXELS (>> 4)
        xram0_struct_set(config_addr, vga_mode4_asprite_t, x_pos_px, enemies[i].x >> 4);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, enemies[i].y >> 4);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, xram_sprite_ptr, sprite_ptr);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, log_size, 4); // 16x16
        xram0_struct_set(config_addr, vga_mode4_asprite_t, has_opacity_metadata, false);
    }
}

void init_sprites(void)
{
    // Define the struct in XRAM first
    RIA.addr0 = SPRITE_CONFIG_ADDR;
    RIA.step0 = 1;

    // Use vga_mode4_asprite_t (Affine Sprite)
    // Initialize with identity transform (Scale 1.0 = 256)
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[0], 256); // SX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[1], 0);   // SHY
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[2], 0);   // TX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[3], 0);   // SHX
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[4], 256); // SY
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[5], 0);   // TY

    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, x_pos_px, reticle_x);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, y_pos_px, reticle_y);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, xram_sprite_ptr, SPRITE_DATA_ADDR);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, log_size, 5); // 32x32 = 2^5
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, has_opacity_metadata, false);

    // Enable Plane 2 for Reticle (Mode 4 Affine)
    xregn(1, 0, 1, 5, 4, 1, SPRITE_CONFIG_ADDR, 1, 2); 

    // --- PLANE 1: ENEMIES (Mode 4 AFFINE) ---
    // Initialize Enemy Configs - Need valid data initially or they might glitch
    for (int i = 0; i < MAX_ENEMIES; i++) {
        unsigned config_addr = ENEMY_CONFIG_BASE + (i * sizeof(vga_mode4_asprite_t)); // SIZE changed
        xram0_struct_set(config_addr, vga_mode4_asprite_t, x_pos_px, -32); // Offscreen
        xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, -32);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, xram_sprite_ptr, ENEMY_DATA_ADDR);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, log_size, 4); // 16x16 = 2^4
        xram0_struct_set(config_addr, vga_mode4_asprite_t, has_opacity_metadata, false);
    }

    // Initialize Worker Configs
    for (int i = 0; i < MAX_WORKERS; i++) {
        unsigned config_addr = WORKER_CONFIG_BASE + (i * sizeof(vga_mode4_asprite_t));
        xram0_struct_set(config_addr, vga_mode4_asprite_t, x_pos_px, -32); // Offscreen
        xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, -32);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, xram_sprite_ptr, WORKER_DATA_ADDR);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, log_size, 4);
        xram0_struct_set(config_addr, vga_mode4_asprite_t, has_opacity_metadata, false);
    }

    // Enable Plane 1 (Enemies AND Workers)
    // Reg 1: Mode = 4
    // Reg 2: Options = 1 (Affine Enabled)
    // Reg 3: Config_Ptr = ENEMY_CONFIG_BASE (Start of block)
    // Reg 4: Length = MAX_ENEMIES + MAX_WORKERS
    // Reg 5: Plane = 1
    xregn(1, 0, 1, 5, 4, 1, ENEMY_CONFIG_BASE, MAX_ENEMIES + MAX_WORKERS, 1);
    
    spawn_enemy(50, 50);
    spawn_enemy(270, 130);
}

void update_reticle_position(int8_t dx, int8_t dy) {
    reticle_x += dx;
    reticle_y += dy;
    
    // Bounds Check (screen is 320x180)
    // Keep the box somewhat on screen
    if (reticle_x < -16) reticle_x = -16;
    if (reticle_x > 320-16) reticle_x = 320-16;
    if (reticle_y < -16) reticle_y = -16;
    if (reticle_y > 180-16) reticle_y = 180-16;
}

void update_sprites(void)
{
    sprite_angle += 1; // 1 degree per frame (256 = 360 approx)
    pulse_time += 4;
    
    // Scale: 1.0 (256) +/- 0.2 (50)
    // sin returns -256..256 (1.0 in 8.8)
    int16_t s_val = SIN_LUT[pulse_time & 0xFF]; // -256..256
    // We want output varying by +/- 50. s_val/5?
    int16_t scale = 256 + (s_val / 5); 
    
    // Angle
    uint8_t angle = (uint8_t)sprite_angle;
    int16_t c = SIN_LUT[(uint8_t)(angle + 64)]; // cos
    int16_t s = SIN_LUT[angle]; // sin
    
    // Affine Matrix 8.8 Fixed Point
    // A = sx * cos(theta)
    // B = sy * -sin(theta)
    // C = sx * sin(theta)
    // D = sy * cos(theta)
    
    // SIN_LUT is 1.0 amplitude (256). Scale is 256 (1.0).
    // result = (scale * sin) >> 8
    
    int16_t A = ((int32_t)scale * c) >> 8; 
    int16_t B = ((int32_t)scale * (-s)) >> 8;
    int16_t C = ((int32_t)scale * s) >> 8;
    int16_t D = ((int32_t)scale * c) >> 8;

    // Centering Logic
    // Screen Position: Top-Left at (reticle_x, reticle_y).
    // Pivot: Map Box Center (16,16) to Texture Center (16,16).

    int16_t cx = 16;
    int16_t cy = 16;
    int32_t center_fixed = 16 << 8; // 4096
    
    // TX = 4096 - (A*16 + B*16)
    int16_t TX = (int16_t)(center_fixed - ((int32_t)A * cx) - ((int32_t)B * cy));
    int16_t TY = (int16_t)(center_fixed - ((int32_t)C * cx) - ((int32_t)D * cy));
    
    // Update XRAM Struct
    RIA.addr0 = SPRITE_CONFIG_ADDR;
    RIA.step0 = 1;
    
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[0], A);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[1], B);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[2], TX);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[3], C); 
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[4], D); 
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, transform[5], TY);
    
    // Update Position Dynamically
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, x_pos_px, reticle_x);
    xram0_struct_set(SPRITE_CONFIG_ADDR, vga_mode4_asprite_t, y_pos_px, reticle_y);
}
