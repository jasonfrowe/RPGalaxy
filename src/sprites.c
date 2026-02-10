#include <rp6502.h>
#include <stdint.h>
#include <math.h> 
#include <stdlib.h>
#include <stdbool.h>

// #include "galaxy.h" // Removed as logic moved to galaxy.c checks
#include "sprites.h"
#include "tables.h"
#include "constants.h"

#include "physics.h"

#define SPRITE_CONFIG_ADDR 0xFD00
#define SPRITE_DATA_ADDR   0xF500 

static uint8_t current_eccentricity = 0; // 0..128


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
            
            workers[i].type = type;
            
            // Set orbit parameters (Geometric)
            // Input x,y is Reticle Top-Left (32x32)
            // We want orbit to be relative to Reticle Center (+16)
            // Screen Center is 160, 90.
            
            int16_t center_x = x + 16;
            int16_t center_y = y + 16;
            
            int16_t cx = 160;
            int16_t cy = 90;
            int16_t dx = center_x - cx;
            int16_t dy = center_y - cy;
            
            // Freeze Eccentricity at Spawn
            workers[i].eccentricity = current_eccentricity;
            
            // Keplerian Parameter Extraction (Rotated Apocenter)
            // We want orbit to pass through (dx, dy) relative to Focus.
            // AND we want (dx, dy) to be the Apocenter (farthest point).
            
            // 1. Calculate Click Angle (Phi)
            uint8_t phi = vector_to_angle(dx, dy);
            
            // 2. Set Omega so Apocenter aligns with Click
            // Std Apocenter is at angle 128 (Left).
            // We want Apocenter at Phi.
            // Angle = Phi.
            // Rotated Angle = Angle - Omega? No.
            // x_rot = x cos w - y sin w.
            // If x is Left (-a(1+e), 0). Angle 128.
            // We want Result to be Direction Phi.
            // - [cos w, sin w] is direction w + 128.
            // So if we want direction Phi, then Phi = w + 128.
            // w = Phi - 128 = Phi + 128 (mod 256).
            workers[i].omega = phi + 128;
            
            // 3. Set Angle to Apocenter
            workers[i].angle = 128 << 8; 
            
            // 4. Calculate Distance r
            int16_t adx = (dx < 0) ? -dx : dx;
            int16_t ady = (dy < 0) ? -dy : dy;
            int16_t r = (adx > ady) ? adx + ady/2 : ady + adx/2;
            
            // 5. Calculate a. r_apo = a(1+e).
            // a = r / (1+e).
            // In 8.8 fixed point: 1.0 = 256.
            // a = (r * 256) / (256 + e).
            
            uint16_t denom = 256 + workers[i].eccentricity;
            int32_t num = (int32_t)r * 256;
            int16_t a_val = (int16_t)(num / denom);
            
            // Clamp Radius
            if (a_val > 85) a_val = 85; 
            if (a_val < 20) a_val = 20;
            
            workers[i].radius = (uint8_t)a_val;
            
            // Velocity Scaling
            uint16_t calc_speed = 3500 / workers[i].radius;
            if (calc_speed > 255) calc_speed = 255;
            if (calc_speed < 20) calc_speed = 20;
            workers[i].speed = (uint8_t)calc_speed;
            
            // Initial position calculate
            update_geometric_orbit(&workers[i].x, &workers[i].y, &workers[i].angle, 
                                   workers[i].radius, current_eccentricity, workers[i].speed, workers[i].omega);

            workers[i].frame = (type == 0) ? 0 : 2; 
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
        // --- GEOMETRIC PHYSICS ---
        update_geometric_orbit(&workers[i].x, &workers[i].y, &workers[i].angle, 
                               workers[i].radius, workers[i].eccentricity, workers[i].speed, workers[i].omega);
        
        // Screen Coords (Center of Sprite)
        // Sprite is 16x16. We must draw at Top-Left.
        // x >> 4 gives Center. Subtract 8.
        int16_t px = ((workers[i].x >> 4) - 8);
        int16_t py = ((workers[i].y >> 4) - 8);
        
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
            
            int16_t cx = 160;
            int16_t cy = 90;
            int16_t dx = x - cx;
            int16_t dy = y - cy;
            
            // Freeze Eccentricity
            enemies[i].eccentricity = current_eccentricity;
            
            // Apocenter Spawn Logic
            uint8_t phi = vector_to_angle(dx, dy);
            enemies[i].omega = phi + 128;
            enemies[i].angle = 128 << 8;
            
            // Distance
            int16_t adx = (dx < 0) ? -dx : dx;
            int16_t ady = (dy < 0) ? -dy : dy;
            int16_t r = (adx > ady) ? adx + ady/2 : ady + adx/2;
            
            // a = r / (1+e)
            uint16_t denom = 256 + enemies[i].eccentricity;
            int32_t num = (int32_t)r * 256;
            int16_t a_val = (int16_t)(num / denom);
            
            if (a_val > 85) a_val = 85; 
            if (a_val < 30) a_val = 30; 
            
            enemies[i].radius = (uint8_t)a_val;
            
            // Speed
            uint16_t calc_speed = 3500 / enemies[i].radius;
            if (calc_speed > 255) calc_speed = 255;
            if (calc_speed < 20) calc_speed = 20;
            enemies[i].speed = (uint8_t)calc_speed;
            
            update_geometric_orbit(&enemies[i].x, &enemies[i].y, &enemies[i].angle, 
                                   enemies[i].radius, current_eccentricity, enemies[i].speed, enemies[i].omega);
            
            enemies[i].timer = 0;
            enemies[i].frame = 0;
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

        // --- GEOMETRIC PHYSICS ---
        update_geometric_orbit(&enemies[i].x, &enemies[i].y, &enemies[i].angle, 
                               enemies[i].radius, enemies[i].eccentricity, enemies[i].speed, enemies[i].omega);
        

        
        // Logic: Rotate based on velocity? Or just spin?
        // Let's spin for now by using the orbital angle (Tidal Locking)
        // enemies[i].angle += 2; // REMOVED: This confounds the orbital position!

        // Animation
        enemies[i].timer++;
        if (enemies[i].timer > 8) { // Animation Speed
            enemies[i].timer = 0;
            enemies[i].frame = (enemies[i].frame + 1) & 3; // Cycle 0-3
        }
        
        // Render - Affine Calculation
        int16_t scale = 256; // 1.0
        // Fix: Use High Byte of 8.8 angle!
        uint8_t angle = (enemies[i].angle >> 8);
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
        // Physics returns Center. Sprite needs Top-Left. 16x16 -> -8.
        xram0_struct_set(config_addr, vga_mode4_asprite_t, x_pos_px, ((enemies[i].x >> 4) - 8));
        xram0_struct_set(config_addr, vga_mode4_asprite_t, y_pos_px, ((enemies[i].y >> 4) - 8));
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
    
    // Calculate Eccentricity based on scale
    // scale max ~307 -> e=0
    // scale min ~205 -> e=0.5 (128)
    // Range = 100.
    // e = (307 - scale) * K.
    // if scale=307, e=0. if scale=205, e=102*1.25 = 128.
    // e = (307 - scale) * 5 / 4.
    
    int16_t e_calc = 307 - scale;
    if (e_calc < 0) e_calc = 0;
    // (e * 5) >> 2 might exceed 16-bit if e is large? 
    // e_calc is max 102. 102*5 = 510. Fits easily.
    e_calc = (e_calc * 5) >> 2;
    if (e_calc > 128) e_calc = 128;
    
    // FIX: Re-enabling dynamic eccentricity as requested by user.
    // Fixed zigzag via safe_mul_shift previously?
    current_eccentricity = (uint8_t)e_calc; 
    // current_eccentricity = 64; // Fixed Ellipse (0.25) 
    
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
