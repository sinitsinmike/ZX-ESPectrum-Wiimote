 #ifndef PS2Keyboard_h
 #define PS2Keyboard_h

#include "def/config.h"

#include "def/hardware.h"
#include "def/keys.h"
#include <Arduino.h>

class PS2Keyboard
{
public:
    static void initialize();
    static void IRAM_ATTR interruptHandler(void);
    static bool isKeymapChanged();
    static bool checkAndCleanKey(uint8_t scancode);

    static void attachInterrupt();
    static void detachInterrupt();

    static uint8_t keymap[256];
    static uint8_t oldmap[256];

    // inject key from wiimote, for not modifying OSD code
    static void emulateKeyChange(uint8_t scancode, uint8_t isdown);
};

#ifdef PS2_KEYB_PRESENT

#define KB_INT_START PS2Keyboard::attachInterrupt()
#define KB_INT_STOP  PS2Keyboard::detachInterrupt()

#else // !PS2_KEYB_PRESENT

#define KB_INT_START
#define KB_INT_STOP

#endif // PS2_KEYB_PRESENT

#endif // PS2Keyboard_h