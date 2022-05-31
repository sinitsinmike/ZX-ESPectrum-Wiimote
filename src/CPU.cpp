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

#include "hardconfig.h"
#include <stdio.h>
#include <string.h>

#include "ESPectrum.h"
#include "Mem.h"
#include "Ports.h"

#include "PS2Kbd.h"
#include "CPU.h"
#include "Config.h"
#include "Tape.h"

#pragma GCC optimize ("O3")

///////////////////////////////////////////////////////////////////////////////

#ifdef CPU_LINKEFONG

#include "Z80_LKF/z80emu.h"
Z80_STATE _zxCpu;

#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef CPU_JLSANCHEZ

#include "Z80_JLS/z80.h"
static bool createCalled = false;
static bool interruptPending = false;

#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef CPU_PER_INSTRUCTION_TIMING

static uint32_t ts_start;
static uint32_t target_frame_micros;
static uint32_t target_frame_cycles;

static inline void begin_timing(uint32_t _target_frame_cycles, uint32_t _target_frame_micros)
{
    target_frame_micros = _target_frame_micros;
    target_frame_cycles = _target_frame_cycles;
    ts_start = micros();
}

static inline void delay_instruction(uint32_t elapsed_cycles)
{
    uint32_t ts_current = micros() - ts_start;
    uint32_t ts_target = target_frame_micros * elapsed_cycles / target_frame_cycles;
    if (ts_target > ts_current) {
        uint32_t us_to_wait = ts_target - ts_current;
        if (us_to_wait < target_frame_micros)
            delayMicroseconds(us_to_wait);
    }
}

#endif  // CPU_PER_INSTRUCTION_TIMING

///////////////////////////////////////////////////////////////////////////////

// machine tstates  f[MHz]   micros
//   48K:   69888 / 3.5    = 19968
//  128K:   70908 / 3.5469 = 19992

uint32_t CPU::statesPerFrame()
{
    if (Config::getArch() == "48K") return 69888;
    else                            return 70908;
}

uint32_t CPU::microsPerFrame()
{
    if (Config::getArch() == "48K") return 19968;
    else                            return 19992;
}

///////////////////////////////////////////////////////////////////////////////

uint32_t CPU::tstates = 0;
uint64_t CPU::global_tstates = 0;

void CPU::setup()
{
    #ifdef CPU_LINKEFONG
    #endif

    #ifdef CPU_JLSANCHEZ
        if (!createCalled)
        {
            Z80::create();
            createCalled = true;
        }
    #endif

    ESPectrum::reset();
}

///////////////////////////////////////////////////////////////////////////////

void CPU::reset() {
    #ifdef CPU_LINKEFONG
        Z80Reset(&_zxCpu);
    #endif

    #ifdef CPU_JLSANCHEZ
        Z80::reset();
    #endif

    global_tstates = 0;
}

///////////////////////////////////////////////////////////////////////////////

void CPU::loop()
{
    uint32_t statesInFrame = statesPerFrame();
    tstates = 0;

    #ifdef CPU_LINKEFONG
        Z80_STATE *state;
        state=&_zxCpu;
        #define DO_Z80_INSTRUCTION (tstates = Z80ExecuteInstruction(&_zxCpu, tstates, NULL))
        #define DO_Z80_INTERRUPT   (Z80Interrupt(&_zxCpu, 0xff, NULL))
    #endif

    #ifdef CPU_JLSANCHEZ
        #define DO_Z80_INSTRUCTION (Z80::execute())
        #define DO_Z80_INTERRUPT   (interruptPending = true)
    #endif
    //Z80ExecuteCycles(&_zxCpu, CalcTStates(), NULL);

    #ifdef CPU_PER_INSTRUCTION_TIMING
        uint32_t prevTstates = 0;
        uint32_t partTstates = 0;
        #define PIT_PERIOD 50
        begin_timing(statesInFrame, microsPerFrame());
    #endif

    tstates = 0;

	while (tstates < statesInFrame)
	{
    
    #ifdef TAPE_TRAPS
    #ifdef CPU_JLSANCHEZ        
        switch (Z80::getRegPC()) {
    #endif
    #ifdef CPU_LINKEFONG
        switch (state->pc) {
    #endif
        case 0x0556: // START LOAD        
            Tape::romLoading=true;
            if (Tape::tapeStatus!=TAPE_LOADING && Tape::tapeFileName!="none") Tape::TAP_Play();
            // Serial.printf("------\n");
            // Serial.printf("TapeStatus: %u\n", Tape::tapeStatus);
            break;
        case 0x04d0: // START SAVE (used for rerouting mic out to speaker in Ports.cpp)
            Tape::SaveStatus=TAPE_SAVING; 
            // Serial.printf("------\n");
            // Serial.printf("SaveStatus: %u\n", Tape::SaveStatus);
            break;
        case 0x053f: // END LOAD / SAVE
            Tape::romLoading=false;
            if (Tape::tapeStatus!=TAPE_STOPPED)
                #ifdef CPU_JLSANCHEZ
                if (Z80::isCarryFlag()) Tape::tapeStatus=TAPE_PAUSED; else Tape::tapeStatus=TAPE_STOPPED;
                #endif
                #ifdef CPU_LINKEFONG                
                if (state->registers.byte[Z80_F] & 0x01) Tape::tapeStatus=TAPE_PAUSED; else Tape::tapeStatus=TAPE_STOPPED;
                #endif
            Tape::SaveStatus=SAVE_STOPPED;
            
            /*
            Serial.printf("TapeStatus: %u\n", Tape::tapeStatus);
            Serial.printf("SaveStatus: %u\n", Tape::SaveStatus);
            #ifdef CPU_JLSANCHEZ
            Serial.printf("Carry Flag: %u\n", Z80::isCarryFlag());            
            #endif
            #ifdef CPU_LINKEFONG
            Serial.printf("Carry Flag: %u\n", state->registers.byte[Z80_F] & 0x01);            
            #endif
            Serial.printf("------\n");
            */

            break;
        }
    #endif

        // frame Tstates before instruction
        uint32_t pre_tstates = tstates;
       
        DO_Z80_INSTRUCTION;

        // frame Tstates after instruction
        //uint32_t post_tstates = tstates;

        // increase global Tstates
        global_tstates += (tstates - pre_tstates);

        #ifdef CPU_PER_INSTRUCTION_TIMING
            if (partTstates > PIT_PERIOD) {
                delay_instruction(tstates);
                partTstates -= PIT_PERIOD;
            } 
            else {
                partTstates += (tstates - prevTstates);
            }
            prevTstates = tstates;
        #endif
	}
    
    #ifdef CPU_PER_INSTRUCTION_TIMING
        delay_instruction(tstates);
    #endif

    DO_Z80_INTERRUPT;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef CPU_JLSANCHEZ

/* Read opcode from RAM */
uint8_t Z80Ops::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    if (ADDRESS_IN_LOW_RAM(address))
        CPU::tstates += CPU::delayContention(CPU::tstates);

    CPU::tstates += 4;
    return Mem::readbyte(address);
}

/* Read/Write byte from/to RAM */
uint8_t Z80Ops::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    if (ADDRESS_IN_LOW_RAM(address))
        CPU::tstates += CPU::delayContention(CPU::tstates);

    CPU::tstates += 3;
    return Mem::readbyte(address);
}
void Z80Ops::poke8(uint16_t address, uint8_t value) {
    // 3 clocks for write byte to RAM
    if (ADDRESS_IN_LOW_RAM(address))
        CPU::tstates += CPU::delayContention(CPU::tstates);

    CPU::tstates += 3;
    Mem::writebyte(address, value);
}

/* Read/Write word from/to RAM */
uint16_t Z80Ops::peek16(uint16_t address) {
    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = Z80Ops::peek8(address);
    uint8_t msb = Z80Ops::peek8(address + 1);
    return (msb << 8) | lsb;
}
void Z80Ops::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    Z80Ops::poke8(address, word.byte8.lo);
    Z80Ops::poke8(address + 1, word.byte8.hi);
}

/* In/Out byte from/to IO Bus */
uint8_t Z80Ops::inPort(uint16_t port) {
    // 3 clocks for read byte from bus
    CPU::tstates += 3;
    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    return Ports::input(loport, hiport);
}
void Z80Ops::outPort(uint16_t port, uint8_t value) {
    // 4 clocks for write byte to bus
    CPU::tstates += 4;
    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    Ports::output(loport, hiport, value);
}

/* Put an address on bus lasting 'tstates' cycles */
void Z80Ops::addressOnBus(uint16_t address, int32_t wstates){
    // Additional clocks to be added on some instructions
    if (ADDRESS_IN_LOW_RAM(address)) {
        for (int idx = 0; idx < wstates; idx++) {
            CPU::tstates += CPU::delayContention(CPU::tstates) + 1;
        }
    }
    else
        CPU::tstates += wstates;
}

/* Clocks needed for processing INT and NMI */
void Z80Ops::interruptHandlingTime(int32_t wstates) {
    CPU::tstates += wstates;
}

/* Callback to know when the INT signal is active */
bool Z80Ops::isActiveINT(void) {
    if (!interruptPending) return false;
    interruptPending = false;
    return true;
}

#endif  // CPU_JLSANCHEZ

///////////////////////////////////////////////////////////////////////////////
