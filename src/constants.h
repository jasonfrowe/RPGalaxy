
#define SCREEN_WIDTH 320U
#define SCREEN_HEIGHT 180U
#define BITMAP_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define PALETTE_ADDR 0xE100U      // 512 Bytes (0xE100 - 0xE300)
#define BITMAP_CONFIG_ADDR 0xE300U // 16 Bytes (0xE300 - 0xE310)
#define PIXEL_DATA_ADDR 0x0000U // Pixel data starts at 0

#define OPL_ADDR        0xFE00  // OPL2 Address port
#define GAMEPAD_INPUT   0xFF78  // XRAM address for gamepad data
#define KEYBOARD_INPUT  0xFFA0  // XRAM address for keyboard data