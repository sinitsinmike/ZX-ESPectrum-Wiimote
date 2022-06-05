///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32
//
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez, Jorge Fuertes and many others
// https://github.com/rampa069/ZX-ESPectrum
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#ifndef ESPectrum_h
#define ESPectrum_h

#include "hardpins.h"
#include <FS.h>

// Declared vars
#ifdef COLOR_3B
#include "ESP32Lib/VGA/VGA3Bit.h"
#include "ESP32Lib/VGA/VGA3BitI.h"
#define VGA VGA3Bit
#endif

#ifdef COLOR_6B
#include "ESP32Lib/VGA/VGA6Bit.h"
#include "ESP32Lib/VGA/VGA6BitI.h"
#define VGA VGA6Bit
#endif

#ifdef COLOR_14B
#include "ESP32Lib/VGA/VGA14Bit.h"
#include "ESP32Lib/VGA/VGA14BitI.h"
#define VGA VGA14Bit
#endif

class ESPectrum
{
public:
    // arduino setup/loop
    static void setup();
    static void loop();

    // reset machine
    static void reset();

    // graphics
    static VGA vga;
    static uint8_t borderColor;
    static uint16_t scanline;
    static int scanoffset;
    static uint8_t lastBorder[312];
    static uint8_t lineChanged[192];
    static uint16_t zxColor(uint8_t color, uint8_t bright);

    static void processKeyboard();

private:
    static void precalcColors();
    static void precalcULASWAP();
    static void precalcborder32();
    static void videoTask(void* unused);
    
};

#endif