#ifndef ESPectrum_h
#define ESPectrum_h

#include "def/hardware.h"
#include "MartianVGA.h"

// Declared vars
#ifdef COLOR_3B
extern VGA3Bit vga;
#endif

#ifdef COLOR_6B
extern VGA6Bit vga;
#endif

#ifdef COLOR_14B
extern VGA14Bit vga;
#endif

class ESPectrum
{
public:
    static void setup();
    static void loop();

    static byte borderColor;
    static uint16_t zxColor(uint8_t color, uint8_t bright);

    static void processKeyboard();
    static void waitForVideoTask();

private:
    static void videoTask(void* unused);
};

#endif