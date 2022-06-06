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

#include "PS2Kbd.h"
#include "Z80_LKF/z80emu.h"
#include "CPU.h"
#include "Z80_LKF/z80user.h"
#include <Arduino.h>

#include "hardpins.h"
#include "messages.h"

#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include <esp_bt.h>

#include "Wiimote2Keys.h"

#include "ESPectrum.h"
#include "FileSNA.h"
#include "Config.h"
#include "FileUtils.h"
#include "osd.h"

#include "Ports.h"
#include "Mem.h"
#include "AySound.h"
#include "Tape.h"

#include "Z80_JLS/z80.h"

// works, but not needed for now
#pragma GCC optimize ("O3")

// EXTERN METHODS
void setup_cpuspeed();

// ESPectrum graphics variables
byte ESPectrum::borderColor = 7;
uint16_t ESPectrum::scanline = 0;
byte ESPectrum::lastBorder[312];
uint8_t ESPectrum::lineChanged[192];
VGA ESPectrum::vga;

volatile byte flashing = 0;
const int SAMPLING_RATE = 44100;
const int BUFFER_SIZE = 2000;

int halfsec, sp_int_ctr, evenframe, updateframe;

// SETUP *************************************
#ifdef AR_16_9
#define VGA_AR_MODE MODE360x200
#endif

#ifdef AR_4_3
#define VGA_AR_MODE MODE320x240
#endif

bool isLittleEndian()
{
	uint16_t val16 = 0x1;
	uint8_t* ptr8 = (uint8_t*)(&val16);
	return (*ptr8 == 1);
}

static uint8_t staticMemPage[0x4000];

static void tryAllocateSRamThenPSRam(uint8_t*& page, const char* pagename)
{
    // only try allocating in SRAM when there are 96K free at least
    if (ESP.getFreeHeap() >= 96*1024)
    {
        page = (uint8_t*)calloc(1, 0x4000);
        if (page != NULL) {
            Serial.printf("Page %s allocated into SRAM (fast)\n", pagename);
            return;
        }
    }
    page = (uint8_t*)ps_calloc(1, 0x4000);
    if (page != NULL) {
        Serial.printf("Page %s allocated into PSRAM (slow)\n", pagename);
        return;
    }
    Serial.printf("ERROR: unable to allocate page %s\n", pagename);
}

void ESPectrum::setup()
{
#ifndef WIIMOTE_PRESENT
    // if no wiimote, turn off peripherals to recover some memory
    esp_bt_controller_deinit();
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif

    Serial.begin(115200);

    Serial.println("ZX-ESPectrum + Wiimote initializing...");

	if (isLittleEndian()) Serial.println("Running on little endian CPU");
	else                  Serial.println("Running on big endian CPU"   );

    if (Config::slog_on) {
        Serial.println(MSG_VGA_INIT);
    }

    Serial.printf("PSRAM size: %d\n", ESP.getPsramSize());
    Serial.printf("Free heap at begin of setup: %d\n", ESP.getFreeHeap());

    initWiimote2Keys();

    Serial.printf("Free heap after wiimote: %d\n", ESP.getFreeHeap());

    FileUtils::initFileSystem();
    Config::load();
    Config::loadSnapshotLists();
    Config::loadTapLists();

    Serial.printf("Free heap after filesystem: %d\n", ESP.getFreeHeap());

    const Mode& vgaMode = Config::aspect_16_9 ? vga.MODE360x200 : vga.MODE320x240;
    OSD::scrW = vgaMode.hRes;
    OSD::scrH = vgaMode.vRes / vgaMode.vDiv;
    Serial.printf("Setting resolution to %d x %d\n", OSD::scrW, OSD::scrH);

#ifdef COLOR_3B
    vga.init(vgaMode, RED_PIN_3B, GRE_PIN_3B, BLU_PIN_3B, HSYNC_PIN, VSYNC_PIN);
#endif

#ifdef COLOR_6B
    const int redPins[] = {RED_PINS_6B};
    const int grePins[] = {GRE_PINS_6B};
    const int bluPins[] = {BLU_PINS_6B};
    vga.init(vgaMode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);
#endif

#ifdef COLOR_14B
    const int redPins[] = {RED_PINS_14B};
    const int grePins[] = {GRE_PINS_14B};
    const int bluPins[] = {BLU_PINS_14B};
    vga.init(vgaMode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);
#endif

    // precalculate colors for current VGA mode
    precalcColors();

    // precalculate ULA SWAP values
    precalcULASWAP();

    // Precalc border 32 bits values
    precalcborder32();

    for (int i=0;i<192;i++) {
        ESPectrum::lineChanged[i]=1;
    }

    vga.clear(0);

    Serial.printf("Free heap after vga: %d \n", ESP.getFreeHeap());

#ifdef BOARD_HAS_PSRAM
    Mem::ram5 = staticMemPage;
    Serial.printf("Page RAM5 statically allocated (fastest)\n");
    tryAllocateSRamThenPSRam(Mem::ram2, "RAM2");
    tryAllocateSRamThenPSRam(Mem::ram0, "RAM0");
    tryAllocateSRamThenPSRam(Mem::rom0, "ROM0");

    tryAllocateSRamThenPSRam(Mem::ram7, "RAM7");
    tryAllocateSRamThenPSRam(Mem::ram1, "RAM1");
    tryAllocateSRamThenPSRam(Mem::ram3, "RAM3");
    tryAllocateSRamThenPSRam(Mem::ram4, "RAM4");
    tryAllocateSRamThenPSRam(Mem::ram6, "RAM6");

    tryAllocateSRamThenPSRam(Mem::rom1, "ROM1");
    tryAllocateSRamThenPSRam(Mem::rom2, "ROM2");
    tryAllocateSRamThenPSRam(Mem::rom3, "ROM3");

    // Allocate RAM for loading TAP files
    Tape::Init();

#else
    Mem::rom0 = (byte *)malloc(16384);

    Mem::ram0 = (byte *)malloc(16384);
    Mem::ram2 = (byte *)malloc(16384);
    Mem::ram5 = (byte *)malloc(16384);
#endif

    Mem::rom[0] = Mem::rom0;
    Mem::rom[1] = Mem::rom1;
    Mem::rom[2] = Mem::rom2;
    Mem::rom[3] = Mem::rom3;

    Mem::ram[0] = Mem::ram0;
    Mem::ram[1] = Mem::ram1;
    Mem::ram[2] = Mem::ram2;
    Mem::ram[3] = Mem::ram3;
    Mem::ram[4] = Mem::ram4;
    Mem::ram[5] = Mem::ram5;
    Mem::ram[6] = Mem::ram6;
    Mem::ram[7] = Mem::ram7;

    Serial.printf("Free heap after allocating emulated ram: %d\n", ESP.getFreeHeap());

#ifdef SPEAKER_PRESENT
    pinMode(SPEAKER_PIN, OUTPUT);
    digitalWrite(SPEAKER_PIN, LOW);
#endif
#ifdef EAR_PRESENT
    pinMode(EAR_PIN, INPUT);
#endif
#ifdef MIC_PRESENT
    pinMode(MIC_PIN, OUTPUT);
    digitalWrite(MIC_PIN, LOW);
#endif

    PS2Keyboard::initialize();

    // START Z80
    Serial.println(MSG_Z80_RESET);
    CPU::setup();

    // make sure keyboard ports are FF
    for (int t = 0; t < 32; t++) {
        Ports::base[t] = 0x1f;
        Ports::wii[t] = 0x1f;
    }

    Serial.printf("%s %u\n", MSG_EXEC_ON_CORE, xPortGetCoreID());

    //vidQueue = xQueueCreate(1, sizeof(uint16_t *));
    //xTaskCreatePinnedToCore(&ESPectrum::videoTask, "videoTask", 1024 * 4, NULL, 5, &videoTaskHandle, 0);

    AySound::initialize();

    Config::requestMachine(Config::getArch(), Config::getRomSet(), true);
    if ((String)Config::ram_file != (String)NO_RAM_FILE) {
        OSD::changeSnapshot(Config::ram_file);
    }

#ifdef ZX_KEYB_PRESENT
    Serial.println("Configuring ZX keyboard pins...");
    // CONFIGURACION PINES TECLADO FISICO:
    const int psKR[] = {AD8, AD9, AD10, AD11, AD12, AD13, AD14, AD15};
    const int psKC[] = {DB0, DB1, DB2, DB3, DB4};
    for (int t = 0; t < 8; t++) {
        pinMode(psKR[t], OUTPUT_OPEN_DRAIN);
    }
    for (int t = 0; t < 5; t++) {
        pinMode(psKC[t], INPUT);
    }
#endif // ZX_KEYB_PRESENT

    Serial.printf("Free heap at end of setup: %d\n", ESP.getFreeHeap());
}

void ESPectrum::reset()
{
    int i;
    for (i = 0; i < 128; i++) {
        Ports::base[i] == 0x1F;
        Ports::wii[i] == 0x1F;
    }

    ESPectrum::borderColor = 7;
    for (i=0;i<312;i++) {
        lastBorder[i]=8; // 8 -> Force repaint of border
    }

    for (i=0;i<192;i++) {
        ESPectrum::lineChanged[i]=1;
    }

    Mem::bankLatch = 0;
    Mem::videoLatch = 0;
    Mem::romLatch = 0;

    if (Config::getArch() == "48K") Mem::pagingLock = 1; else Mem::pagingLock = 0;  
    
    Mem::modeSP3 = 0;
    Mem::romSP3 = 0;
    Mem::romInUse = 0;

    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    CPU::reset();
}

#define NUM_SPECTRUM_COLORS 16

static word spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

void ESPectrum::precalcColors()
{
    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++)
        spectrum_colors[i] = (spectrum_colors[i] & vga.RGBAXMask) | vga.SBits;
}

uint16_t ESPectrum::zxColor(uint8_t color, uint8_t bright) {
    if (bright) color += 8;
    return spectrum_colors[color];
}

// VIDEO core 0 *************************************

#define SPEC_W 256
#define SPEC_H 192

static int calcY(int offset);
static int calcX(int offset);
static void swap_flash(word *a, word *b);

// Precalc ULA_SWAP
static int offBmp[SPEC_H];
static int offAtt[SPEC_H];
#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))
void ESPectrum::precalcULASWAP()
{
    for (int i = 0; i < SPEC_H; i++) {
        offBmp[i] = ULA_SWAP(i) << 5;
        offAtt[i] = ((i >> 3) << 5) + 0x1800;
    }
}

uint8_t border;

// Precalc border 32 bits values
static uint32_t border32[8];

void ESPectrum::precalcborder32()
{
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

// for abbreviating evaluation of convenience keys
static inline void evalConvKey(uint8_t key, uint8_t p1, uint8_t b1, uint8_t p2, uint8_t b2)
{
    uint8_t specialKeyState = PS2Keyboard::keymap[key];
    if (specialKeyState != PS2Keyboard::oldmap[key])
    {
        bitWrite(Ports::base[p1], b1, specialKeyState);
        bitWrite(Ports::base[p2], b2, specialKeyState);
    }
}

/* Load zx keyboard lines from PS/2 */
void ESPectrum::processKeyboard() {
    uint8_t kempston = 0;

    bitWrite(Ports::base[0], 0, PS2Keyboard::keymap[0x12]);
    bitWrite(Ports::base[0], 1, PS2Keyboard::keymap[0x1a]);
    bitWrite(Ports::base[0], 2, PS2Keyboard::keymap[0x22]);
    bitWrite(Ports::base[0], 3, PS2Keyboard::keymap[0x21]);
    bitWrite(Ports::base[0], 4, PS2Keyboard::keymap[0x2a]);

    bitWrite(Ports::base[1], 0, PS2Keyboard::keymap[0x1c]);
    bitWrite(Ports::base[1], 1, PS2Keyboard::keymap[0x1b]);
    bitWrite(Ports::base[1], 2, PS2Keyboard::keymap[0x23]);
    bitWrite(Ports::base[1], 3, PS2Keyboard::keymap[0x2b]);
    bitWrite(Ports::base[1], 4, PS2Keyboard::keymap[0x34]);

    bitWrite(Ports::base[2], 0, PS2Keyboard::keymap[0x15]);
    bitWrite(Ports::base[2], 1, PS2Keyboard::keymap[0x1d]);
    bitWrite(Ports::base[2], 2, PS2Keyboard::keymap[0x24]);
    bitWrite(Ports::base[2], 3, PS2Keyboard::keymap[0x2d]);
    bitWrite(Ports::base[2], 4, PS2Keyboard::keymap[0x2c]);

    bitWrite(Ports::base[3], 0, PS2Keyboard::keymap[0x16]);
    bitWrite(Ports::base[3], 1, PS2Keyboard::keymap[0x1e]);
    bitWrite(Ports::base[3], 2, PS2Keyboard::keymap[0x26]);
    bitWrite(Ports::base[3], 3, PS2Keyboard::keymap[0x25]);
    bitWrite(Ports::base[3], 4, PS2Keyboard::keymap[0x2e]);

    bitWrite(Ports::base[4], 0, PS2Keyboard::keymap[0x45]);
    bitWrite(Ports::base[4], 1, PS2Keyboard::keymap[0x46]);
    bitWrite(Ports::base[4], 2, PS2Keyboard::keymap[0x3e]);
    bitWrite(Ports::base[4], 3, PS2Keyboard::keymap[0x3d]);
    bitWrite(Ports::base[4], 4, PS2Keyboard::keymap[0x36]);

    bitWrite(Ports::base[5], 0, PS2Keyboard::keymap[0x4d]);
    bitWrite(Ports::base[5], 1, PS2Keyboard::keymap[0x44]);
    bitWrite(Ports::base[5], 2, PS2Keyboard::keymap[0x43]);
    bitWrite(Ports::base[5], 3, PS2Keyboard::keymap[0x3c]);
    bitWrite(Ports::base[5], 4, PS2Keyboard::keymap[0x35]);

    bitWrite(Ports::base[6], 0, PS2Keyboard::keymap[0x5a]);
    bitWrite(Ports::base[6], 1, PS2Keyboard::keymap[0x4b]);
    bitWrite(Ports::base[6], 2, PS2Keyboard::keymap[0x42]);
    bitWrite(Ports::base[6], 3, PS2Keyboard::keymap[0x3b]);
    bitWrite(Ports::base[6], 4, PS2Keyboard::keymap[0x33]);

    bitWrite(Ports::base[7], 0, PS2Keyboard::keymap[0x29]);
    bitWrite(Ports::base[7], 1, PS2Keyboard::keymap[0x14]);
    bitWrite(Ports::base[7], 2, PS2Keyboard::keymap[0x3a]);
    bitWrite(Ports::base[7], 3, PS2Keyboard::keymap[0x31]);
    bitWrite(Ports::base[7], 4, PS2Keyboard::keymap[0x32]);

    // Kempston joystick
#ifdef PS2_ARROWKEYS_AS_KEMPSTON
    Ports::base[0x1f] = 0;
    bitWrite(Ports::base[0x1f], 0, !PS2Keyboard::keymap[KEY_CURSOR_RIGHT]);
    bitWrite(Ports::base[0x1f], 1, !PS2Keyboard::keymap[KEY_CURSOR_LEFT]);
    bitWrite(Ports::base[0x1f], 2, !PS2Keyboard::keymap[KEY_CURSOR_DOWN]);
    bitWrite(Ports::base[0x1f], 3, !PS2Keyboard::keymap[KEY_CURSOR_UP]);
    bitWrite(Ports::base[0x1f], 4, !PS2Keyboard::keymap[KEY_ALT_GR]);
#endif // PS2_ARROWKEYS_AS_KEMPSTON    

    // Cursor keys
    uint8_t specialKeyState;
#ifdef PS2_ARROWKEYS_AS_CURSOR
    evalConvKey(KEY_CURSOR_LEFT,  0, 0, 3, 4);    // LEFT ARROW  (Shift + 5)
    evalConvKey(KEY_CURSOR_DOWN,  0, 0, 4, 4);    // DOWN ARROW  (Shift + 6)
    evalConvKey(KEY_CURSOR_UP,    0, 0, 4, 3);    // UP ARROW    (Shift + 7)
    evalConvKey(KEY_CURSOR_RIGHT, 0, 0, 4, 2);    // RIGHT ARROW (Shift + 8)
#endif // PS2_ARROWKEYS_AS_CURSOR

    // Convenience keys for english keyboard
#ifdef PS2_CONVENIENCE_KEYS_EN
    evalConvKey(KEY_BACKSPACE, 0, 0, 4, 0);    // BKSP (Shift + 0)
    evalConvKey(KEY_EQUAL,     7, 1, 6, 1);    // =    (Ctrl + L)
    evalConvKey(KEY_MINUS,     7, 1, 6, 3);    // -    (Ctrl + J)
    evalConvKey(KEY_SEMI,      7, 1, 5, 1);    // ;    (Ctrl + O)
    evalConvKey(KEY_APOS,      7, 1, 4, 3);    // '    (Ctrl + 7)
    evalConvKey(KEY_COMMA,     7, 1, 7, 3);    // ,    (Ctrl + N)
    evalConvKey(KEY_DOT,       7, 1, 7, 2);    // .    (Ctrl + M)
    evalConvKey(KEY_DIV,       7, 1, 0, 4);    // /    (Ctrl + V)
#endif // PS2_CONVENIENCE_KEYS_EN

    // Convenience keys for spanish keyboard
#ifdef PS2_CONVENIENCE_KEYS_ES
    evalConvKey(102,  0, 0, 4, 0);    // BKSP (Shift + 0)
    evalConvKey(91,   7, 1, 6, 2);    // +    (Ctrl + K)
    evalConvKey(82,   7, 1, 4, 3);    // '    (Ctrl + 7)
    evalConvKey(65,   7, 1, 7, 3);    // ,    (Ctrl + N)
    evalConvKey(73,   7, 1, 7, 2);    // .    (Ctrl + M)
    evalConvKey(74,   7, 1, 6, 3);    // -    (Ctrl + J)
#endif // PS2_CONVENIENCE_KEYS_ES
}

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

/* +-------------+
   | LOOP core 1 |
   +-------------+
 */

int vgaX;
int ulaX;       // from 0 to 32
int bmpOffset;  // offset for bitmap in graphic memory
int attOffset;  // offset for attrib in graphic memory
int att, bmp;   // attribute and bitmap
int bri;        // bright flag
int back, fore; // background and foreground colors

uint8_t palette[2]; //0 backcolor 1 Forecolor
uint8_t a0,a1,a2,a3;
uint32_t brd;
uint8_t* grmem;
uint32_t* lineptr32;
uint8_t* lineptr;

bool firstHalf;

int ESPectrum::scanoffset = 0;

void ESPectrum::loop() {

    if (halfsec) {
        flashing = ~flashing;
    }
    sp_int_ctr++;
    halfsec = !(sp_int_ctr % 25);

    processKeyboard();
    updateWiimote2Keys();
    
    OSD::do_OSD();

#ifdef LOG_DEBUG_TIMING 
    uint32_t ts_start = micros();
#endif

    uint32_t statesInFrame = CPU::statesPerFrame();

    #ifdef CPU_PER_INSTRUCTION_TIMING
        uint32_t prevTstates = 0;
        uint32_t partTstates = 0;
        #define PIT_PERIOD 50
        begin_timing(statesInFrame, CPU::microsPerFrame());
    #endif

    CPU::tstates = 0;
    int statesPerLine = 224;
    int scanlinestates;
    int scanlinestatesR;

    scanline = 1;
    int scanlineR = 0;    
//    scanlinestates = 32;
//    scanlinestatesR = 144;

//    scanlinestates = scanoffset % statesPerLine;
//    scanlinestatesR = (scanlinestates + 112) % statesPerLine;

//    scanlinestates = 235; // 235 as start value seems ok for left half of main screen
    scanlinestates = 11; // 235 as start value seems ok for left half of main screen
    scanlinestatesR = 144;

    static int ctr2 = 0;
    if (ctr2 == 0) {
        ctr2 = 50;
        Serial.printf("Scanline: %u; Scanoffset: %u; Scanlinestates: %d; ScanlinestatesR: %d\n", scanline,scanoffset,scanlinestates,scanlinestatesR);
    }
    else ctr2--;


    while (CPU::tstates < statesInFrame)
	{

        // frame Tstates before instruction
        uint32_t pre_tstates = CPU::tstates;
        
        Z80::execute();
        
        #ifdef CPU_PER_INSTRUCTION_TIMING
        if (partTstates > PIT_PERIOD) {
            delay_instruction(CPU::tstates);
            partTstates -= PIT_PERIOD;
        } 
        else {
            partTstates += (CPU::tstates - prevTstates);
        }
        prevTstates = CPU::tstates;
        #endif

        CPU::global_tstates += (CPU::tstates - pre_tstates); // increase global Tstates

        scanlinestates += (CPU::tstates - pre_tstates);
        scanlinestatesR += (CPU::tstates - pre_tstates);        

        // wait to (almost) correct tstate before beginning line render
        if (scanlinestates >= statesPerLine) {

            if (scanline>59 && scanline<64) {

                // Top border
                if (lastBorder[scanline]!=borderColor) {
                    lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);
                    for (int i = 0; i < 90; i++) {
                        *lineptr32 = border32[borderColor];
                        lineptr32++;
                    }
                    lastBorder[scanline]=borderColor;
                }
            }

            if (scanline>63 && scanline<256) {
                
                lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);

                // Left border
                if (lastBorder[scanline]!=borderColor) {
                    for (int i = 0; i < 13; i++) {
                        *lineptr32 = border32[borderColor];
                        lineptr32++;
                    }
                } else lineptr32+=13;


                // Main screen
                //if (ESPectrum::lineChanged[scanline-64]) {

                    grmem = Mem::videoLatch ? Mem::ram7 : Mem::ram5;
                    
                    bmpOffset = offBmp[(scanline - 60) - 4];
                    attOffset = offAtt[(scanline - 60) - 4];
                    
                    for (ulaX = 0; ulaX < 16; ulaX++) // foreach byte in line
//                    for (ulaX = 0; ulaX < 32; ulaX++) // foreach byte in line                    
                    {
                        att = grmem[attOffset++];  // get attribute byte

                        bri = att & 0x40;
                        fore = zxColor(att & 0b111, bri);
                        back = zxColor((att >> 3) & 0b111, bri);
                        if ((att >> 7) && flashing) {
                            palette[0] = fore; palette[1] = back;
                        } else {
                            palette[0] = back; palette[1] = fore;
                        }

                        bmp = grmem[bmpOffset++];  // get bitmap byte

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

                    }

                    //ESPectrum::lineChanged[scanline-64] &= 0xfe;

                    lineptr32+=32;

                //} else lineptr32+=64;
                
                // Right border
                if (lastBorder[scanline]!=borderColor) {
                    for (int i = 0; i < 13; i++) {
                        *lineptr32 = border32[borderColor];
                        lineptr32++;                    
                    }
                    lastBorder[scanline]=borderColor;
                }
            }

            if (scanline>255 && scanline<260) {
                
                if (lastBorder[scanline]!=borderColor) {
                    // Bottom border
                    lineptr32 = (uint32_t *)(vga.backBuffer[scanline-60]);
                    for (int i = 0; i < 90; i++) {
                        *lineptr32 = border32[borderColor];
                        lineptr32++;
                    }
                    lastBorder[scanline]=borderColor;
                }
            }

            scanline++;
            scanlinestates -= statesPerLine; 
            
        }

        // wait to (almost) correct tstate before beginning line render
        if ((scanlinestatesR >= statesPerLine)) {

            if (scanlineR>63 && scanlineR<256) {
                
                // Main screen
//                if (ESPectrum::lineChanged[scanline-64]) {

                    lineptr32 = (uint32_t *)(vga.backBuffer[scanlineR-60]);
                    
                    grmem = Mem::videoLatch ? Mem::ram7 : Mem::ram5;
                    
                    bmpOffset = offBmp[(scanlineR - 60) - 4];
                    attOffset = offAtt[(scanlineR - 60) - 4];
                    
                    bmpOffset+=16;
                    attOffset+=16;
                    lineptr32+=45;

                    for (ulaX = 0; ulaX < 16; ulaX++) // foreach byte in line
                    {
                        att = grmem[attOffset++];  // get attribute byte

                        bri = att & 0x40;
                        fore = zxColor(att & 0b111, bri);
                        back = zxColor((att >> 3) & 0b111, bri);
                        if ((att >> 7) && flashing) {
                            palette[0] = fore; palette[1] = back;
                        } else {
                            palette[0] = back; palette[1] = fore;
                        }

                        bmp = grmem[bmpOffset++];  // get bitmap byte

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

                    }

  //                  ESPectrum::lineChanged[scanline-64] &= 0xfe;

//                }
                
            }

            scanlineR++;
            scanlinestatesR -= statesPerLine; 

        }

	}

    #ifdef CPU_PER_INSTRUCTION_TIMING
        delay_instruction(CPU::tstates);
    #endif
    
    CPU::setintpending();

#ifdef LOG_DEBUG_TIMING    

    uint32_t ts_end = micros();
    uint32_t elapsed = ts_end - ts_start;
    uint32_t target = CPU::microsPerFrame();
    uint32_t idle = target - elapsed;
    // if (idle < target)
    //     delayMicroseconds(idle);

    static int ctr = 0;
    if (ctr == 0) {
        ctr = 50;
        Serial.printf("[CPUTask] elapsed: %u; idle: %u\n", elapsed, idle);
        Serial.printf("Scanline: %u; Scanoffset: %u\n", scanline,scanoffset);
    }
    else ctr--;

#endif

    AySound::update();

}
