
#define BITMAP_DATA 0xE100U // 320x180x16
#define SCREEN_WIDTH 320U
#define SCREEN_HEIGHT 180U
#define BITMAP_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define PALETTE_ADDR 0xE200U  // Palette address in XRAM (shifted to avoid struct collision)
#define PIXEL_DATA_ADDR 0x0000U // Pixel data starts at 0

#define OPL_ADDR        0xFE00  // OPL2 Address port
#define GAMEPAD_INPUT   0xFF78  // XRAM address for gamepad data
#define KEYBOARD_INPUT  0xFFA0  // XRAM address for keyboard data