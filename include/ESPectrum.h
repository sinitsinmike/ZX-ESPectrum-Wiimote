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

// #ifdef COLOR_6B
// #include "vga_6bit.h"
// #define VGA VGA6Bit
// #endif

#ifdef COLOR_14B
#include "ESP32Lib/VGA/VGA14Bit.h"
#include "ESP32Lib/VGA/VGA14BitI.h"
#define VGA VGA14Bit
#endif

// #define ESP_AUDIO_FREQ 44800
// #define ESP_AUDIO_SAMPLES 896
// #define ESP_AUDIO_TSTATES 78

// #define ESP_AUDIO_FREQ 31200
// #define ESP_AUDIO_SAMPLES 624
// #define ESP_AUDIO_TSTATES 112

// #define ESP_AUDIO_FREQ 22400
// #define ESP_AUDIO_SAMPLES 448
// #define ESP_AUDIO_TSTATES 156

#define ESP_AUDIO_FREQ 11200
#define ESP_AUDIO_SAMPLES 224
#define ESP_AUDIO_TSTATES 312

class ESPectrum
{
public:
    // arduino setup/loop
    static void setup();
    static void loop();

    // reset machine
    static void reset();

    static VGA vga;
    static uint8_t borderColor;
    static void processKeyboard();

    static char audioBuffer[1024];
    static signed char aud_volume;

    static int ESPoffset;
   
};

#endif