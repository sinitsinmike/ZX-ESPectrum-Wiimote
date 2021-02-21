#ifndef ESPectrum_hardware_h
#define ESPectrum_hardware_h

// adjusted for Lilygo TTGO
#define SPEAKER_PIN 25
#define EAR_PIN 34
#define MIC_PIN 0

// adjusted for Lilygo TTGO
#define KEYBOARD_DATA 32
#define KEYBOARD_CLK 33

/////////////////////////////////////////////////
// Storage mode
/////////////////////////////////////////////////
// define ONLY one of these
// USE_INT_FLASH for internal flash storage
// USE_SD_CARD for external SD card

#define USE_INT_FLASH 1
//#define USE_SD_CARD 1

// check: only one must be defined
#if defined(USE_INT_FLASH) && defined(USE_SD_CARD)
#error "Only one of (USE_INT_FLASH, USE_SD_CARD) must be defined"
#endif

// Storage mode: pins for external SD card
#ifdef USE_SD_CARD
// adjusted for Lilygo TTGO
#define SDCARD_CS 13
#define SDCARD_MOSI 12
#define SDCARD_MISO 2
#define SDCARD_CLK 14
#endif

/////////////////////////////////////////////////
// Color mode
/////////////////////////////////////////////////
// define ONLY one of these
// COLOR_3B for -----BGR
// COLOR_6B for --BBGGRR
// COLOR_14B for --BBBBGG GGGRRRRR

// #define COLOR_3B
#define COLOR_6B
// #define COLOR_14B

// check: only one must be defined
#if (defined(COLOR_3B) && defined(COLOR_6B)) || (defined(COLOR_6B) && defined(COLOR_14B)) || defined(COLOR_14B) && defined(COLOR_3B)
#error "Only one of (COLOR_3B, COLOR_6B, COLOR_14B) must be defined"
#endif


/////////////////////////////////////////////////
// 3 bit pins
#define RED_PIN_3B 22
#define GRE_PIN_3B 19
#define BLU_PIN_3B 5

/////////////////////////////////////////////////
// 6 bit pins
// adjusted for Lilygo TTGO
#define RED_PINS_6B 21, 22
#define GRE_PINS_6B 18, 19
#define BLU_PINS_6B  4,  5

/////////////////////////////////////////////////
// 14 bit pins
#define RED_PINS_14B 21, 21, 21, 21, 22
#define GRE_PINS_14B 18, 18, 18, 18, 19
#define BLU_PINS_14B      4,  4,  4,  5

/////////////////////////////////////////////////
// VGA sync pins
#define HSYNC_PIN 23
#define VSYNC_PIN 15

/////////////////////////////////////////////////
// Colors for 3 bit mode
#ifdef COLOR_3B           //       BGR 
#define BLACK   0x08      // 0000 1000
#define BLUE    0x0C      // 0000 1100
#define RED     0x09      // 0000 1001
#define MAGENTA 0x0D      // 0000 1101
#define GREEN   0x0A      // 0000 1010
#define CYAN    0x0E      // 0000 1110
#define YELLOW  0x0B      // 0000 1011
#define WHITE   0x0F      // 0000 1111
#endif

/////////////////////////////////////////////////
// Colors for 6 bit mode
#ifdef COLOR_6B               //   BB GGRR 
#define BLACK       0xC0      // 1100 0000
#define BLUE        0xE0      // 1110 0000
#define RED         0xC2      // 1100 0010
#define MAGENTA     0xE2      // 1110 0010
#define GREEN       0xC8      // 1100 1000
#define CYAN        0xE8      // 1110 1000
#define YELLOW      0xCA      // 1100 1010
#define WHITE       0xEA      // 1110 1010
                              //   BB GGRR 
#define BRI_BLACK   0xC0      // 1100 0000
#define BRI_BLUE    0xF0      // 1111 0000
#define BRI_RED     0xC3      // 1100 0011
#define BRI_MAGENTA 0xF3      // 1111 0011
#define BRI_GREEN   0xCC      // 1100 1100
#define BRI_CYAN    0xFC      // 1111 1100
#define BRI_YELLOW  0xCF      // 1100 1111
#define BRI_WHITE   0xFF      // 1111 1111
#endif

/////////////////////////////////////////////////
// Colors for 14 bit mode
#ifdef COLOR_14B              //   BB --GG ---R R---
#define BRI_BLACK   0xC000    // 1100 0000 0000 0000
#define BRI_BLUE    0xF000    // 1111 0000 0000 0000
#define BRI_RED     0xC018    // 1100 0000 0001 1000
#define BRI_MAGENTA 0xF018    // 1111 0000 0001 1000
#define BRI_GREEN   0xC300    // 1100 0011 0000 0000
#define BRI_CYAN    0xF300    // 1111 0011 0000 0000
#define BRI_YELLOW  0xC318    // 1100 0011 0001 1000
#define BRI_WHITE   0xF318    // 1111 0011 0001 1000
                              //   BB --GG ---R R---
#define BLACK       0xC000    // 1100 0000 0000 0000
#define BLUE        0xE000    // 1110 0000 0000 0000
#define RED         0xC010    // 1100 0000 0001 0000
#define MAGENTA     0xE010    // 1110 0000 0001 0000
#define GREEN       0xC200    // 1100 0010 0000 0000
#define CYAN        0xE200    // 1110 0010 0000 0000
#define YELLOW      0xC210    // 1100 0010 0001 0000
#define WHITE       0xE210    // 1110 0010 0001 0000

#endif

#endif // ESPectrum_hardware_h
