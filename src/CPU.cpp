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
//
// Delay Contention: for emulating CPU slowing due to sharing bus with ULA
// NOTE: Only 48K spectrum contention implemented. This function must be called
// only when dealing with affected memory (use ADDRESS_IN_LOW_RAM macro)
//
// delay contention: emulates wait states introduced by the ULA (graphic chip)
// whenever there is a memory access to contended memory (shared between ULA and CPU).
// detailed info: https://worldofspectrum.org/faq/reference/48kreference.htm#ZXSpectrum
// from paragraph which starts with "The 50 Hz interrupt is synchronized with..."
// if you only read from https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
// without reading the previous paragraphs about line timings, it may be confusing.
//
static unsigned char wait_states[8] = { 6, 5, 4, 3, 2, 1, 0, 0 }; // sequence of wait states
static unsigned char IRAM_ATTR delayContention(unsigned int currentTstates)
{

    // delay states one t-state BEFORE the first pixel to be drawn
    currentTstates++;

	// each line spans 224 t-states
	unsigned short int line = currentTstates / 224; // int line

    // only the 192 lines between 64 and 255 have graphic data, the rest is border
	if (line < 64 || line >= 256) return 0;

	// only the first 128 t-states of each line correspond to a graphic data transfer
	// the remaining 96 t-states correspond to border
	unsigned char halfpix = currentTstates % 224; // int halfpix

	if (halfpix >= 128) return 0;

    return(wait_states[halfpix & 0x07]);

}

///////////////////////////////////////////////////////////////////////////////

static void ula_contend_port_early( uint16_t port )
{

    if ( ( port & 49152 ) == 16384 )
        ALU_video(delayContention(CPU::tstates)+1);
    else
        ALU_video(1);

}

void ula_contend_port_late( uint16_t port )
{

  if( (port & 0x0001) == 0x00) {
        ALU_video(delayContention(CPU::tstates)+3);
  } else {
    if ( (port & 49152) == 16384 ) {
   		ALU_video(delayContention(CPU::tstates)+1);
	    ALU_video(delayContention(CPU::tstates)+1);
		ALU_video(delayContention(CPU::tstates)+1);
	} else {
	    ALU_video(3);
	}
 
  }

}

#ifdef CPU_JLSANCHEZ

/* Read opcode from RAM */
uint8_t IRAM_ATTR Z80Ops::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    if (ADDRESS_IN_LOW_RAM(address))
        ALU_video(delayContention(CPU::tstates) + 4);        
    else
        ALU_video(4);

    return Mem::readbyte(address);
}

/* Read/Write byte from/to RAM */
uint8_t IRAM_ATTR Z80Ops::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    if (ADDRESS_IN_LOW_RAM(address))
        ALU_video(delayContention(CPU::tstates) + 3);
    else
        ALU_video(3);

    return Mem::readbyte(address);
}
void IRAM_ATTR Z80Ops::poke8(uint16_t address, uint8_t value) {
    if (ADDRESS_IN_LOW_RAM(address)) {
       
        ALU_video(delayContention(CPU::tstates) + 3);

        // #ifndef NO_VIDEO
        // if (address < 0x5800) { // Bitmap
        //     lineChanged[address & 0x3fff] |= 0x01;
        // } else if (address < 0x5b00) { // Attr
        //     byte chgdata;
        //     uint16_t addrvid = address & 0x3fff;
        //     int rowbase = ((addrvid - 0x1800) >> 5) << 3;
        //     if (value & 0x80) chgdata=0x03; else chgdata=0x01; // Flashing bit on
        //     for (int i=0;i<8;i++){
        //         lineChanged[offBmp[rowbase + i] + (addrvid & 0x1f)] |= chgdata;
        //     }
        // }
        // #endif

    } else 
        ALU_video(3);

    Mem::writebyte(address, value);
}

/* Read/Write word from/to RAM */
uint16_t IRAM_ATTR Z80Ops::peek16(uint16_t address) {
    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = Z80Ops::peek8(address);
    uint8_t msb = Z80Ops::peek8(address + 1);
    return (msb << 8) | lsb;
}
void IRAM_ATTR Z80Ops::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    Z80Ops::poke8(address, word.byte8.lo);
    Z80Ops::poke8(address + 1, word.byte8.hi);
}

/* In/Out byte from/to IO Bus */
uint8_t IRAM_ATTR Z80Ops::inPort(uint16_t port) {
    
    //ula_contend_port_early( port );   // Contended I/O
    //ula_contend_port_late( port );    // Contended I/O
    
    // 3 clocks for read byte from bus (4 according to https://worldofspectrum.org/faq/reference/48kreference.htm#IOContention)
    ALU_video(4); // No contended I/O
    
    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    return Ports::input(loport, hiport);

}

void IRAM_ATTR Z80Ops::outPort(uint16_t port, uint8_t value) {
    
    // 4 clocks for write byte to bus
    ALU_video(4); // No contended I/O
  
    //ula_contend_port_early( port );   // Contended I/O

    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    Ports::output(loport, hiport, value);

    //ula_contend_port_late( port );    // Contended I/O

}

/* Put an address on bus lasting 'tstates' cycles */
void IRAM_ATTR Z80Ops::addressOnBus(uint16_t address, int32_t wstates){
    // Additional clocks to be added on some instructions
    if (ADDRESS_IN_LOW_RAM(address)) {
        for (int idx = 0; idx < wstates; idx++)
            ALU_video(delayContention(CPU::tstates) + 1);
    }
    else
        ALU_video(wstates);

}

/* Clocks needed for processing INT and NMI */
void IRAM_ATTR Z80Ops::interruptHandlingTime(int32_t wstates) {
    ALU_video(wstates);
}

/* Callback to know when the INT signal is active */
bool IRAM_ATTR Z80Ops::isActiveINT(void) {
    if (!interruptPending) return false;
    interruptPending = false;
    return true;
}

#endif  // CPU_JLSANCHEZ

///////////////////////////////////////////////////////////////////////////////
//  VIDEO EMULATION
///////////////////////////////////////////////////////////////////////////////
#define NUM_SPECTRUM_COLORS 16
static word spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

// static word specfast_colors[128]; // Array for faster color calc in ALU_video

void precalcColors() {
    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++) {
        spectrum_colors[i] = (spectrum_colors[i] & ESPectrum::vga.RGBAXMask) | ESPectrum::vga.SBits;
    }

    // // Calc array for faster color calcs in ALU_video
    // for (int i = 0; i < (NUM_SPECTRUM_COLORS >> 1); i++) {
    //     // Normal
    //     specfast_colors[i] = spectrum_colors[i];
    //     specfast_colors[i << 3] = spectrum_colors[i];
    //     // Bright
    //     specfast_colors[i | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
    //     specfast_colors[(i << 3) | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
    // }

}

uint16_t zxColor(uint8_t color, uint8_t bright) {
    if (bright) color += 8;
    return spectrum_colors[color];
}

// Precalc ULA_SWAP
#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))
void precalcULASWAP() {
    for (int i = 0; i < SPEC_H; i++) {
        offBmp[i] = ULA_SWAP(i) << 5;
        offAtt[i] = ((i >> 3) << 5) + 0x1800;
    }
}

// Precalc border 32 bits values
static unsigned int border32[8];
void precalcborder32()
{
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

void ALU_video_init() {

    precalcColors();    // precalculate colors for current VGA mode

    precalcULASWAP();   // precalculate ULA SWAP values

    precalcborder32();  // Precalc border 32 bits values

    // for (int i=0;i<6144;i++) lineChanged[i]=1; // Mark screen changed array as dirty

}

void ALU_video_reset() {

    // for (int i=0;i<6144;i++) lineChanged[i]=1; // Mark screen changed array as dirty

}

// //             if (scanline>59 && scanline<64) {
// //                 // Top border
// //                 if (lastBorder[scanline]!=borderColor) {
// //                     lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);
// //                     for (int i = 0; i < 90; i++) {
// //                         *lineptr32 = border32[borderColor];
// //                         lineptr32++;
// //                     }
// //                     lastBorder[scanline]=borderColor;
// //                 }
// //             }
// //             if (scanline>63 && scanline<256) {
// //                 lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);
// //                 // Left border
// //                 if (lastBorder[scanline]!=borderColor) {
// //                     for (int i = 0; i < 13; i++) {
// //                         *lineptr32 = border32[borderColor];
// //                         lineptr32++;
// //                     }
// //                 } else lineptr32+=13;

// //                 // Main screen
// //                 lineptr32+=64;
// //                 
// //                 // Right border
// //                 if (lastBorder[scanline]!=borderColor) {
// //                     for (int i = 0; i < 13; i++) {
// //                         *lineptr32 = border32[borderColor];
// //                         lineptr32++;                    
// //                     }
// //                     lastBorder[scanline]=borderColor;
// //                 }
// //             }
// //             if (scanline>255 && scanline<260) {
// //                 if (lastBorder[scanline]!=borderColor) {
// //                     // Bottom border
// //                     lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);
// //                     for (int i = 0; i < 90; i++) {
// //                         *lineptr32 = border32[borderColor];
// //                         lineptr32++;
// //                     }
// //                     lastBorder[scanline]=borderColor;
// //                 }
// //             }

///////////////////////////////////////////////////////////////////////////////
//  VIDEO DRAW FUNCTION
///////////////////////////////////////////////////////////////////////////////
static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory
static unsigned int att, bmp;   // attribute and bitmap
static unsigned int back, fore; // background and foreground colors
static unsigned int palette[2]; //0 backcolor 1 Forecolor
static unsigned int a0,a1,a2,a3;

static uint8_t* grmem;
static uint32_t* lineptr32;

#define TSTATES_PER_LINE 224

#define TS_PHASE_1 13415 // START OF VISIBLE ULA DRAW @ 360x200, SCANLINE 60
#define TS_PHASE_2 14311 // START OF LEFT BORDER OF TOP LEFT CORNER OF MAINSCREEN, SCANLINE 64
#define TS_PHASE_3 57319 // START OF BOTTOM BORDER, SCANLINE 256

// DrawStatus values
#define TOPBORDER_BLANK 0
#define TOPBORDER 1
#define MAINSCREEN_BLANK 2
#define LEFTBORDER 3
#define LINEDRAW 4
#define RIGHTBORDER 5
#define BOTTOMBORDER_BLANK 6
#define BOTTOMBORDER 7
#define BLANK 8

static unsigned int tstateDraw=TS_PHASE_1; // Drawing start point (in Tstates)
static unsigned char DrawStatus=TOPBORDER_BLANK;
static unsigned int linedraw_cnt;
static unsigned int coldraw_cnt;
static unsigned int ALU_video_rest;
static unsigned int brd;

///////////////////////////////////////////////////////////////////////////////
static void IRAM_ATTR ALU_video(unsigned int statestoadd) {

    CPU::tstates += statestoadd;
    
    if (DrawStatus==TOPBORDER_BLANK) {

        if (CPU::tstates > tstateDraw) {

            statestoadd = CPU::tstates - tstateDraw;
            tstateDraw += TSTATES_PER_LINE;

            lineptr32 = (uint32_t *)(ESPectrum::vga.backBuffer[linedraw_cnt]);
            lineptr32 += 1; // Border offset for 360x200
            coldraw_cnt = 0;

            DrawStatus = TOPBORDER;

        }

    }

    if (DrawStatus==TOPBORDER) {

        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4

        brd = ESPectrum::borderColor;

        for (int i=0; i < (statestoadd >> 2); i++) {

            *lineptr32++ = border32[brd];
            *lineptr32++ = border32[brd];            

            coldraw_cnt++;

            if (coldraw_cnt > 43) {
                DrawStatus = TOPBORDER_BLANK;
                ALU_video_rest=0;
                linedraw_cnt++;
                if (linedraw_cnt > 3) {
                    DrawStatus = MAINSCREEN_BLANK;
                    tstateDraw= TS_PHASE_2;
                }
                return;
            }

        }

        return;

    }

    if (DrawStatus==MAINSCREEN_BLANK) {
     
        if (CPU::tstates > tstateDraw) {

            statestoadd = CPU::tstates - tstateDraw;
            tstateDraw += TSTATES_PER_LINE;
            lineptr32 = (uint32_t *)(ESPectrum::vga.backBuffer[linedraw_cnt]);
            lineptr32 += 1; // Border offset for 360x200
            coldraw_cnt = 0;
            DrawStatus = LEFTBORDER;

        }

    }    

    if (DrawStatus==LEFTBORDER) {
     
        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4
        
        brd = ESPectrum::borderColor;

        for (int i=0; i < (statestoadd >> 2); i++) {    

            *lineptr32++ = border32[brd];
            *lineptr32++ = border32[brd];

            coldraw_cnt++;

            if (coldraw_cnt > 5) {  
                
                DrawStatus = LINEDRAW;
                
                bmpOffset = offBmp[linedraw_cnt - 4];
                attOffset = offAtt[linedraw_cnt - 4];

                grmem = Mem::videoLatch ? Mem::ram7 : Mem::ram5;

                // This is, maybe, more correct but it seems not needed
                // statestoadd -= ((i + 1) << 2);
                // ALU_video_rest += statestoadd;

                ALU_video_rest = 0;

                return;

            }

        }

        return;

    }    

    if (DrawStatus == LINEDRAW) {

        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4

        for (int i=0; i < (statestoadd >> 2); i++) {    

            // if (lineChanged[bmpOffset] !=0) {
            
                att = grmem[attOffset];  // get attribute byte

                fore = spectrum_colors[(att & 0b111) | ((att & 0x40) >> 3)];
                back = spectrum_colors[((att >> 3) & 0b111) | ((att & 0x40) >> 3)];                

                if ((att >> 7) && ESPectrum::flashing) {
                    palette[0] = fore;
                    palette[1] = back;
                } else {
                    palette[0] = back;
                    palette[1] = fore;
                }

                //
                // This seems faster but it isn't. Why?
                //
                // if ((att >> 7) && ESPectrum::flashing) {
                //     palette[0] = specfast_colors[att & 0x47];
                //     palette[1] = specfast_colors[att & 0x78];
                // } else {
                //     palette[0] = specfast_colors[att & 0x78];
                //     palette[1] = specfast_colors[att & 0x47];
                // }

                bmp = grmem[bmpOffset];  // get bitmap byte

                a0 = palette[(bmp >> 7) & 0x01];
                a1 = palette[(bmp >> 6) & 0x01];
                a2 = palette[(bmp >> 5) & 0x01];
                a3 = palette[(bmp >> 4) & 0x01];
                *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

                a0 = palette[(bmp >> 3) & 0x01];
                a1 = palette[(bmp >> 2) & 0x01];
                a2 = palette[(bmp >> 1) & 0x01];
                a3 = palette[bmp & 0x01];
                *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

            //     lineChanged[bmpOffset] &= 0xfe;
            // } else {
            //     lineptr32 += 2;
            // }

            attOffset++;
            bmpOffset++;

            coldraw_cnt++;

            if (coldraw_cnt > 37) {

                DrawStatus = RIGHTBORDER;

                // // This is, maybe, more correct but it seems not needed
                // statestoadd -= ((i+1) << 2);
                // ALU_video_rest += statestoadd;

                ALU_video_rest = 0;

                return;

            }

        }

        return;

    }

    if (DrawStatus==RIGHTBORDER) {
     
        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4

        brd = ESPectrum::borderColor;

        for (int i=0; i < (statestoadd >> 2); i++) {

            *lineptr32++ = border32[brd];
            *lineptr32++ = border32[brd];

            coldraw_cnt++;

            if (coldraw_cnt > 43 ) { 

                DrawStatus = MAINSCREEN_BLANK;
                linedraw_cnt++;
                ALU_video_rest=0;

                if (linedraw_cnt > 195) {
                    DrawStatus = BOTTOMBORDER_BLANK;
                    tstateDraw = TS_PHASE_3;
                }
        
                return;

            }

        }

        return;

    }    

    if (DrawStatus==BOTTOMBORDER_BLANK) {

            if (CPU::tstates > tstateDraw) {

                statestoadd = CPU::tstates - tstateDraw;
                tstateDraw += TSTATES_PER_LINE;

                lineptr32 = (uint32_t *)(ESPectrum::vga.backBuffer[linedraw_cnt]);
                lineptr32 += 1; // Border offset for 360x200
                coldraw_cnt = 0;

                DrawStatus = BOTTOMBORDER;

            }

    }

    if (DrawStatus==BOTTOMBORDER) {

        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4

        brd = ESPectrum::borderColor;

        for (int i=0; i < (statestoadd >> 2); i++) {    

            *lineptr32++ = border32[brd];
            *lineptr32++ = border32[brd];

            coldraw_cnt++;

            if (coldraw_cnt > 43) {
                DrawStatus = BOTTOMBORDER_BLANK;
                ALU_video_rest=0;
                linedraw_cnt++;
                if (linedraw_cnt > 199) {
                    DrawStatus = BLANK;
                    tstateDraw= TS_PHASE_1;
                }
                return;
            }

        }

        return;

    }

    if (DrawStatus==BLANK) {
     
         if (CPU::tstates < tstateDraw) {
            linedraw_cnt=0;
            DrawStatus=TOPBORDER_BLANK;
        }
    
    }

}

///////////////////////////////////////////////////////////////////////////////
// static void IRAM_ATTR ALU_video(unsigned int statestoadd) {

//     CPU::tstates += statestoadd;

// #ifndef NO_VIDEO
//     if (DrawStatus==0) {

//         if (CPU::tstates > tstateDraw) {
            
//             statestoadd = CPU::tstates - tstateDraw;
//             tstateDraw += TSTATES_PER_LINE;
            
//             lineptr32 = (uint32_t *)(ESPectrum::vga.backBuffer[linedraw_cnt + 4]);
//             lineptr32 += 13;
            
//             bmpOffset = offBmp[linedraw_cnt];
//             attOffset = offAtt[linedraw_cnt];

//             grmem = Mem::videoLatch ? Mem::ram7 : Mem::ram5;

//             coldraw_cnt = 0;

//             DrawStatus = 1;

//         }

//     } 

//     if (DrawStatus == 1 /* LINEDRAW */) {

//         statestoadd += ALU_video_rest;
//         ALU_video_rest = statestoadd & 0x03; // Mod 4

//         for (int i=0; i < (statestoadd >> 2); i++) {    

//             // if (lineChanged[bmpOffset] !=0) {
            
//                 att = grmem[attOffset];  // get attribute byte

//                 fore = spectrum_colors[(att & 0b111) | ((att & 0x40) >> 3)];
//                 back = spectrum_colors[((att >> 3) & 0b111) | ((att & 0x40) >> 3)];                

//                 if ((att >> 7) && ESPectrum::flashing) {
//                     palette[0] = fore;
//                     palette[1] = back;
//                 } else {
//                     palette[0] = back;
//                     palette[1] = fore;
//                 }

//                 //
//                 // This seems faster but it isn't. Why?
//                 //
//                 // if ((att >> 7) && ESPectrum::flashing) {
//                 //     palette[0] = specfast_colors[att & 0x47];
//                 //     palette[1] = specfast_colors[att & 0x78];
//                 // } else {
//                 //     palette[0] = specfast_colors[att & 0x78];
//                 //     palette[1] = specfast_colors[att & 0x47];
//                 // }

//                 bmp = grmem[bmpOffset];  // get bitmap byte

//                 a0 = palette[(bmp >> 7) & 0x01];
//                 a1 = palette[(bmp >> 6) & 0x01];
//                 a2 = palette[(bmp >> 5) & 0x01];
//                 a3 = palette[(bmp >> 4) & 0x01];
//                 *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

//                 a0 = palette[(bmp >> 3) & 0x01];
//                 a1 = palette[(bmp >> 2) & 0x01];
//                 a2 = palette[(bmp >> 1) & 0x01];
//                 a3 = palette[bmp & 0x01];
//                 *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

//             //     lineChanged[bmpOffset] &= 0xfe;
//             // } else {
//             //     lineptr32 += 2;
//             // }

//             attOffset++;
//             bmpOffset++;

//             coldraw_cnt++;

//             if (coldraw_cnt & 0x20) {  // coldraw_cnt > 32
//                 DrawStatus = 0;
//                 ALU_video_rest=0;
//                 linedraw_cnt++;
//                 if (linedraw_cnt > 191) DrawStatus = 2;
//                 break;
//             }

//         }

//     } else if (DrawStatus==2) {
        
//         if (CPU::tstates < TSTATES_TLC_MAINSCR) {
//             tstateDraw=TSTATES_TLC_MAINSCR;
//             linedraw_cnt=0;
//             DrawStatus=0;
//         }
    
//     }
// #endif

// }