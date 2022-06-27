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
#include "CPU.h"
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
#include "CPU.h"
#include "AySound.h"
#include "Tape.h"

#include "Z80_JLS/z80.h"

#include "pwm_audio.h"
#include "LPF.h"

// works, but not needed for now
#pragma GCC optimize ("O3")

// EXTERN METHODS
void setup_cpuspeed();

//const int SAMPLING_RATE = 44100;
//const int BUFFER_SIZE = 2000;

// SETUP *************************************
// ESPectrum graphics variables
VGA ESPectrum::vga;
byte ESPectrum::borderColor = 7;

// Audio variables
unsigned char ESPectrum::audioBuffer[2][2184];
unsigned char ESPectrum::overSamplebuf[2184*2];
signed char ESPectrum::aud_volume = -8;
int ESPectrum::ESPoffset=ESP_DELAY_OFFSET;
int ESPectrum::buffertofill=1;
int ESPectrum::buffertoplay=0;
size_t written;
//uint8_t *audbufptr;

static QueueHandle_t secondTaskQueue;
static TaskHandle_t secondTaskHandle;
static uint8_t *param;

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

    ALU_video_init();

    borderColor = 0;

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

    // Init tape
    Tape::Init();

#ifdef SPEAKER_PRESENT
//    pinMode(SPEAKER_PIN, OUTPUT);
//    digitalWrite(SPEAKER_PIN, LOW);
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

    secondTaskQueue = xQueueCreate(1, sizeof(uint8_t *));
    xTaskCreatePinnedToCore(&ESPectrum::secondTask, "secondTask", 4096, NULL, 5, &secondTaskHandle, 0);

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
   
    // pwm_audio_config_t pac;
    // pac.duty_resolution    = LEDC_TIMER_8_BIT;
    // pac.gpio_num_left      = 25;
    // pac.ledc_channel_left  = LEDC_CHANNEL_0;
    // pac.gpio_num_right     = -1;
    // pac.ledc_channel_right = LEDC_CHANNEL_1;
    // pac.ledc_timer_sel     = LEDC_TIMER_0;
    // pac.tg_num             = TIMER_GROUP_0;
    // pac.timer_num          = TIMER_0;
    // pac.ringbuf_len        = 1024 * 8;

    // pwm_audio_init(&pac);             /**< Initialize pwm audio */
    // pwm_audio_set_param(ESP_AUDIO_FREQ,LEDC_TIMER_8_BIT,1);
    // pwm_audio_start();                 /**< Start to run */
    // pwm_audio_set_volume(aud_volume);

//    audbufptr = (uint8_t *) audioBuffer[buffertoplay];

    setCpuFrequencyMhz(240);

    Serial.printf("Free heap at end of setup: %d\n", ESP.getFreeHeap());
}

void ESPectrum::reset()
{
    int i;
    for (i = 0; i < 128; i++) {
        Ports::base[i] == 0x1F;
        Ports::wii[i] == 0x1F;
    }

    borderColor = 7;

    ALU_video_reset();

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

void IRAM_ATTR ESPectrum::secondTask(void *unused) {

    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_8_BIT;
    pac.gpio_num_left      = 25;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;

    pwm_audio_init(&pac);             /**< Initialize pwm audio */
    pwm_audio_set_param(ESP_AUDIO_FREQ,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();                 /**< Start to run */
    pwm_audio_set_volume(aud_volume);

    for (;;) {
        xQueueReceive(secondTaskQueue, &param, portMAX_DELAY);
        pwm_audio_write(param, ESP_AUDIO_SAMPLES, &written, portMAX_DELAY);
    }

}


/* +-------------+
   | LOOP core 1 |
   +-------------+
 */

#if (defined(LOG_DEBUG_TIMING) && defined(SHOW_FPS))
static double totalseconds=0;
#endif

void ESPectrum::loop() {

    processKeyboard();
    
    updateWiimote2Keys();
    
    OSD::do_OSD();

    param = (uint8_t *) audioBuffer[buffertoplay];
    xQueueSend(secondTaskQueue, &param, portMAX_DELAY);

#if defined(LOG_DEBUG_TIMING) || defined(VIDEO_FRAME_TIMING)
    uint32_t ts_start = micros();
#endif

    CPU::loop();    

    // // Finish fill of oversampled audio buffer
    if (CPU::audbufcnt < ESP_AUDIO_OVERSAMPLES) {
        int signal = CPU::lastaudioBit ? 255: 0;
        for (int i=CPU::audbufcnt; i < ESP_AUDIO_OVERSAMPLES;i++) overSamplebuf[i] = signal;
    }

    //Downsample (median)
    int fval;
    for (int i=0;i<ESP_AUDIO_OVERSAMPLES;i+=8) {    
        fval  =  overSamplebuf[i];
        fval +=  overSamplebuf[i+1];
        fval +=  overSamplebuf[i+2];
        fval +=  overSamplebuf[i+3];
        fval +=  overSamplebuf[i+4];
        fval +=  overSamplebuf[i+5];
        fval +=  overSamplebuf[i+6];
        fval +=  overSamplebuf[i+7];
        audioBuffer[buffertofill][i>>3] = fval >> 3;
    }

#if defined(LOG_DEBUG_TIMING) || defined(VIDEO_FRAME_TIMING)
    uint32_t ts_end = micros();
    uint32_t elapsed = ts_end - ts_start;
    uint32_t target = CPU::microsPerFrame();
    int32_t idle = target - elapsed;
#endif

#ifdef VIDEO_FRAME_TIMING
    //if (idle + ESPoffset > 0) delayMicroseconds(idle + ESPoffset); // FOR TESTING PURPOSE ONLY
    if (idle > 0) delayMicroseconds(idle);   
#endif
#ifdef LOG_DEBUG_TIMING
    static int ctr = 0;
    static int ctrcount = 0;
    static uint32_t sumelapsed;
    #ifdef SHOW_FPS
        totalseconds += elapsed;
    #endif
    if (ctr == 0) {
        ctr = 10;
        sumelapsed+=elapsed;
        ctrcount++;
        if ((ctrcount & 0x000F) == 0) {
            Serial.printf("========================================\n");
            Serial.printf("[CPU] elapsed: %u; idle: %d; offset: %d\n", elapsed, idle, ESPoffset);
            Serial.printf("[Audio] Volume: %d\n", aud_volume);
            Serial.printf("[CPU] average: %u; samples: %u\n", sumelapsed / ctrcount, ctrcount);     
            Serial.printf("[Beeper samples taken] %u\n", CPU::audbufcnt);  
            #ifdef SHOW_FPS
                Serial.printf("[Framecnt] %u; [Seconds] %f; [FPS] %f\n", CPU::framecnt, totalseconds / 1000000, CPU::framecnt / (totalseconds / 1000000));
                totalseconds = 0;
                CPU::framecnt = 0;
            #endif
        }
    }
    else ctr--;
#endif

    // Swap audio buffers
    buffertofill ^= 1;
    buffertoplay ^= 1;

    AySound::update();

    vTaskDelay(0); // important to avoid task watchdog timeouts - change this to slow down emu

}

