#include "def/config.h"

#include "def/hardware.h"
#include "def/keys.h"
#include <Arduino.h>

#ifdef PS2_KEYB_PRESENT

#define KB_INT_START attachInterrupt(digitalPinToInterrupt(KEYBOARD_CLK), kb_interruptHandler, FALLING)
#define KB_INT_STOP detachInterrupt(digitalPinToInterrupt(KEYBOARD_CLK))

#else

#define KB_INT_START
#define KB_INT_STOP

#endif // PS2_KEYB_PRESENT

extern byte lastcode;

void IRAM_ATTR kb_interruptHandler(void);
void kb_begin();
boolean isKeymapChanged();
boolean checkAndCleanKey(uint8_t scancode);

// inject key from wiimote, for not modifying OSD code
void emulateKeyChange(uint8_t scancode, uint8_t isdown);

