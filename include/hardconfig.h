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

///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32 microcontroller
//
// hardconfig.h
// hardcoded configuration file (@compile time) for ZX-ESPectrum
// created by David Crespo on 19/11/2020
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ESPectrum_hardconfig_h
#define ESPectrum_hardconfig_h

///////////////////////////////////////////////////////////////////////////////
// CPU core selection
//
// one of the following MUST be defined:
// - CPU_LINKEFONG: use LinKeFong's core, faster but less precise 
// - CPU_JLSANCHEZ: use JLSanchez's core, slower but more precise
///////////////////////////////////////////////////////////////////////////////

// #define CPU_LINKEFONG
#define CPU_JLSANCHEZ

///////////////////////////////////////////////////////////////////////////////
// CPU timing configuration

// #define CPU_PER_INSTRUCTION_TIMING for precise CPU timing, undefine it
// to let the emulator run free (and too fast :). If #defined,
// delayMicros() will be called after few instructions so CPU runs almost realtime.
///////////////////////////////////////////////////////////////////////////////

#define CPU_PER_INSTRUCTION_TIMING

// CPU_PIT_PERIOD controls every how many cycles (approx) delayMicros() is called.
// The lowest the value, the highest the precision, but there is a small performance
// penalty when calling delayMicros() which may lead to worse timing in some games.
// I recommend NOT changing the default value of 50, except for better PWM sound.
#define CPU_PIT_PERIOD 50
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Video timing configuration
//
// #define VIDEO_FRAME_TIMING for precise video timing, limiting to 50fps.
// undefine it to let the emulator run free (and too fast :)
///////////////////////////////////////////////////////////////////////////////

#define VIDEO_FRAME_TIMING

// LOG_DEBUG_TIMING generates simple timing log messages to console very second.
// #define LOG_DEBUG_TIMING
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Video color depth
//
// one of the following MUST be defined:
// - COLOR_3B: 3 bit color, (RGB), using 3 pins and 1 byte per pixel
// - COLOR_6B: 6 bit color, (RRGGBB), using 6 pins and 1 byte per pixel
// - COLOR_14B: 14 bit color (RRRRRGGGGGBBBB), using 14 pins and 2 bytes per pixel
///////////////////////////////////////////////////////////////////////////////

#define COLOR_3B
// #define COLOR_6B
// #define COLOR_14B

// check: only one must be defined
#if (defined(COLOR_3B) && defined(COLOR_6B)) || (defined(COLOR_6B) && defined(COLOR_14B)) || defined(COLOR_14B) && defined(COLOR_3B)
#error "Only one of (COLOR_3B, COLOR_6B, COLOR_14B) must be defined"
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Screen aspect ratio (16/9 or 4/3)
//
// define ONLY one of these
// AR_16_9
// AR_4_3
///////////////////////////////////////////////////////////////////////////////

#define AR_16_9
// #define AR_4_3

// check: only one must be defined
#if (defined(AR_16_9) && defined(AR_4_3))
#error "Only one of (AR_16_9, AR_4_3) must be defined"
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Resolution, border and centering
//
// Total number of pixels drawn affects video task timing,
// so don't be tempted to draw borders to fill screen, it would ruin timing!
//
// BOR_W and BOR_H are the actual border pixels drawn outside of image.
// OFF_X and OFF_Y are used for centering, use with caution;
// you could write off the buffer and crash the emulator.
///////////////////////////////////////////////////////////////////////////////
#ifdef AR_16_9
#define BOR_W 52
#define BOR_H 4
#define OFF_X 0
#define OFF_Y 0
// if you can't center the image in your screen,
// set some offset, (ex: OFF_X = _20_)
// use a smaller border (ex: BOR_W = 32 == 52 - _20_)
// then change OFF_X for software centering (0 < OFF_X < 40) (40 == 2 * _20_)
#endif

#ifdef AR_4_3
#define BOR_W 32
#define BOR_H 24
#define OFF_X 0
#define OFF_Y 0
#endif

///////////////////////////////////////////////////////////////////////////////
// Storage mode
//
// define ONLY one of these
// USE_INT_FLASH for internal flash storage
// USE_SD_CARD for external SD card
///////////////////////////////////////////////////////////////////////////////

#define USE_INT_FLASH 1
//#define USE_SD_CARD 1

// check: only one must be defined
#if defined(USE_INT_FLASH) && defined(USE_SD_CARD)
#error "Only one of (USE_INT_FLASH, USE_SD_CARD) must be defined"
#endif
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// PS/2 Keyboard
//
// define PS2_KEYB_PRESENT if you want PS/2 Keyboard to be enabled.
//
// also, if you happen to have a pesky keyboard which won't initialize
// until it receives an echo message, also define PS2_KEYB_FORCE_INIT

#define PS2_KEYB_PRESENT
#define PS2_KEYB_FORCE_INIT
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Wiimote Support
//
// define WIIMOTE_PRESENT if you want Wiimote to be supported
// also, you should define wiimote button mappings for each game.
//

#define WIIMOTE_PRESENT
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Physical ZX SPectrum 40-keys / 8-outputs / 5-inputs keyboard
//
// define ZX_KEYB_PRESENT if you want the good old speccy keyboard to be present.
// 

// #define ZX_KEYB_PRESENT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Audio I/O
//
// define SPEAKER_PRESENT if you want the speaker to be present.
// define EAR_PRESENT if you want the ear input port to be present.
// define MIC_PRESENT if you want the mic output port to be present.
// 

#define SPEAKER_PRESENT
// #define EAR_PRESENT
// #define MIC_PRESENT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Audio I/O
//
// define USE_AY_SOUND if you want to use AY-3-891X emulation thru FabGL.
// 

#define USE_AY_SOUND
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Snapshot loading behaviour
//
// define SNAPSHOT_LOAD_FORCE_ARCH if you want the architecture present
// in the snapshot file to be forcibly set as the current emulated machine.
// 
// if current machine is 48K and a 128K snapshot is loaded,
// arch will be switched to 128K always (with SINCLAIR romset as the default).
//
// but if machine is in 128K mode and a 48K snapshot is loaded,
// arch will be switched to 48K only if SNAPSHOT_LOAD_FORCE_ARCH is defined.

#define SNAPSHOT_LOAD_FORCE_ARCH
///////////////////////////////////////////////////////////////////////////////

#endif // ESPectrum_config_h
