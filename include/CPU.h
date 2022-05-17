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

#ifndef CPU_h
#define CPU_h

#include <inttypes.h>

class CPU
{
public:
    // call this for initializing CPU
    static void setup();

    // call this for executing a frame's worth of instructions
    static void loop();

    // call this for resetting the CPU
    static void reset();

    // get the number of CPU Tstates per frame (machine dependant)
    static uint32_t statesPerFrame();

    // get the number of microseconds per frame (machine dependant)
    static uint32_t microsPerFrame();

    // CPU Tstates elapsed in current frame
    static uint32_t tstates;

    // CPU Tstates elapsed since reset
    static uint64_t global_tstates;

    // Delay Contention: for emulating CPU slowing due to sharing bus with ULA
    // NOTE: Only 48K spectrum contention implemented. This function must be called
    // only when dealing with affected memory (use ADDRESS_IN_LOW_RAM macro)
    static uint8_t delayContention(uint32_t currentTstates);
};

///////////////////////////////////////////////////////////////////////////////
//
// delay contention: emulates wait states introduced by the ULA (graphic chip)
// whenever there is a memory access to contended memory (shared between ULA and CPU).
// detailed info: https://worldofspectrum.org/faq/reference/48kreference.htm#ZXSpectrum
// from paragraph which starts with "The 50 Hz interrupt is synchronized with..."
// if you only read from https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
// without reading the previous paragraphs about line timings, it may be confusing.
//
inline uint8_t CPU::delayContention(uint32_t currentTstates)
{
    // sequence of wait states
    static uint8_t wait_states[8] = { 6, 5, 4, 3, 2, 1, 0, 0 };

	// delay states one t-state BEFORE the first pixel to be drawn
	currentTstates += 1;

	// each line spans 224 t-states
	int line = currentTstates / 224;

	// only the 192 lines between 64 and 255 have graphic data, the rest is border
	if (line < 64 || line >= 256) return 0;

	// only the first 128 t-states of each line correspond to a graphic data transfer
	// the remaining 96 t-states correspond to border
	int halfpix = currentTstates % 224;
	if (halfpix >= 128) return 0;

	int modulo = halfpix % 8;
	return wait_states[modulo];
}



#endif // CPU_h
