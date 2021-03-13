#include "PS2Kbd.h"
#include "z80emu/z80emu.h"
#include "z80main.h"
#include "z80user.h"
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

#include "AySound.h"

// EXTERN VARS
extern CONTEXT _zxContext;
extern Z80_STATE _zxCpu;
extern int _total;
extern int _next_total;

// EXTERN METHODS

void setup_cpuspeed();

// ESPectrum graphics variables
byte ESPectrum::borderColor = 7;
VGA ESPectrum::vga;

// Memory
uint8_t* Mem::rom0 = NULL;
uint8_t* Mem::rom1 = NULL;
uint8_t* Mem::rom2 = NULL;
uint8_t* Mem::rom3 = NULL;

uint8_t* Mem::ram0 = NULL;
uint8_t* Mem::ram1 = NULL;
uint8_t* Mem::ram2 = NULL;
uint8_t* Mem::ram3 = NULL;
uint8_t* Mem::ram4 = NULL;
uint8_t* Mem::ram5 = NULL;
uint8_t* Mem::ram6 = NULL;
uint8_t* Mem::ram7 = NULL;

volatile uint8_t Mem::bankLatch = 0;
volatile uint8_t Mem::videoLatch = 0;
volatile uint8_t Mem::romLatch = 0;
volatile uint8_t Mem::pagingLock = 0;
uint8_t Mem::modeSP3 = 0;
uint8_t Mem::romSP3 = 0;
uint8_t Mem::romInUse = 0;

// Ports
volatile uint8_t Ports::base[128];
volatile uint8_t Ports::wii[128];

volatile byte flashing = 0;
volatile byte tick;
const int SAMPLING_RATE = 44100;
const int BUFFER_SIZE = 2000;

int halfsec, sp_int_ctr, evenframe, updateframe;

static QueueHandle_t vidQueue;
static TaskHandle_t videoTaskHandle;
static volatile bool videoTaskIsRunning = false;    // volatile keyword REQUIRED!!!
static uint16_t *param;

// SETUP *************************************
#ifdef AR_16_9
#define VGA_AR_MODE MODE360x200
#endif

#ifdef AR_4_3
#define VGA_AR_MODE MODE320x240
#endif

void ESPectrum::setup()
{
#ifndef WIIMOTE_PRESENT
    // if no wiimote, turn off peripherals to recover some memory
    esp_bt_controller_deinit();
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif

    Serial.begin(115200);

    Serial.println("ZX-ESPectrum + Wiimote initializing...");

    if (Config::slog_on) {
        Serial.println(MSG_VGA_INIT);
    }

    Serial.printf("PSRAM size: %d\n", ESP.getPsramSize());
    Serial.printf("HEAP BEGIN %d\n", ESP.getFreeHeap());

    initWiimote2Keys();

    Serial.printf("HEAP AFTER WIIMOTE %d\n", ESP.getFreeHeap());

    FileUtils::initFileSystem();
    Config::load();
    Config::loadSnapshotLists();

    Serial.printf("HEAP AFTER FILESYSTEM %d\n", ESP.getFreeHeap());

#ifdef BOARD_HAS_PSRAM
    Mem::rom0 = (byte *)ps_malloc(16384);
    Mem::rom1 = (byte *)ps_malloc(16384);
    Mem::rom2 = (byte *)ps_malloc(16384);
    Mem::rom3 = (byte *)ps_malloc(16384);

    Mem::ram0 = (byte *)ps_malloc(16384);
    Mem::ram1 = (byte *)ps_malloc(16384);
    Mem::ram2 = (byte *)ps_malloc(16384);
    Mem::ram3 = (byte *)ps_malloc(16384);
    Mem::ram4 = (byte *)ps_malloc(16384);
    Mem::ram5 = (byte *)malloc(16384);
    Mem::ram6 = (byte *)ps_malloc(16384);
    Mem::ram7 = (byte *)malloc(16384);
#else
    Mem::rom0 = (byte *)malloc(16384);

    Mem::ram0 = (byte *)malloc(16384);
    Mem::ram2 = (byte *)malloc(16384);
    Mem::ram5 = (byte *)malloc(16384);
#endif

    Serial.printf("HEAP AFTER RAM %d\n", ESP.getFreeHeap());

#ifdef COLOR_3B
    vga.init(vga.VGA_AR_MODE, RED_PIN_3B, GRE_PIN_3B, BLU_PIN_3B, HSYNC_PIN, VSYNC_PIN);
#endif

#ifdef COLOR_6B
    const int redPins[] = {RED_PINS_6B};
    const int grePins[] = {GRE_PINS_6B};
    const int bluPins[] = {BLU_PINS_6B};
    vga.init(vga.VGA_AR_MODE, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);
#endif

#ifdef COLOR_14B
    const int redPins[] = {RED_PINS_14B};
    const int grePins[] = {GRE_PINS_14B};
    const int bluPins[] = {BLU_PINS_14B};
    vga.init(vga.VGA_AR_MODE, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);
#endif

    Serial.printf("HEAP after vga  %d \n", ESP.getFreeHeap());

    vga.clear(0);

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

    Serial.printf("%s bank %u: %ub\n", MSG_FREE_HEAP_AFTER, 0, ESP.getFreeHeap());
    Serial.printf("CALC TSTATES/PERIOD %u\n", CalcTStates());

    // START Z80
    Serial.println(MSG_Z80_RESET);
    zx_setup();

    // make sure keyboard ports are FF
    for (int t = 0; t < 32; t++) {
        Ports::base[t] = 0x1f;
        Ports::wii[t] = 0x1f;
    }

    Serial.printf("%s %u\n", MSG_EXEC_ON_CORE, xPortGetCoreID());
    Serial.printf("%s Z80 RESET: %ub\n", MSG_FREE_HEAP_AFTER, ESP.getFreeHeap());

    vidQueue = xQueueCreate(1, sizeof(uint16_t *));
    xTaskCreatePinnedToCore(&ESPectrum::videoTask, "videoTask", 1024 * 4, NULL, 5, &videoTaskHandle, 0);

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

    Serial.println("End of setup");
}

// VIDEO core 0 *************************************

#define SPEC_W 256
#define SPEC_H 192

static int calcY(int offset);
static int calcX(int offset);
static void swap_flash(word *a, word *b);

void ESPectrum::videoTask(void *unused) {
    unsigned int ff, i, byte_offset;
    unsigned char color_attrib, pixel_map, flash, bright;
    unsigned int zx_vidcalc, calc_y;

    word zx_fore_color, zx_back_color, tmp_color;
    // byte active_latch;

    videoTaskIsRunning = true;
    uint16_t *param;

    while (1) {
        xQueuePeek(vidQueue, &param, portMAX_DELAY);
        if ((int)param == 1)
            break;

        for (unsigned int vga_lin = 0; vga_lin < BOR_H+SPEC_H+BOR_H; vga_lin++) {
            // tick = 0;
            if (vga_lin < BOR_H || vga_lin >= BOR_H + SPEC_H) {

                for (int bor = OFF_X; bor < OFF_X+BOR_W+SPEC_W+BOR_W; bor++)
                    vga.dotFast(bor, vga_lin+OFF_Y, zxColor(borderColor, 0));
            }
            else
            {
                for (int bor = OFF_X; bor < OFF_X+BOR_W; bor++) {
                    vga.dotFast(bor, vga_lin+OFF_Y, zxColor(borderColor, 0));
                    vga.dotFast(bor + SPEC_W+BOR_W, vga_lin+OFF_Y, zxColor(borderColor, 0));
                }

                for (ff = 0; ff < 32; ff++) // foreach byte in line
                {

                    byte_offset = (vga_lin - BOR_H) * 32 + ff;
                    calc_y = calcY(byte_offset);
                    if (!Mem::videoLatch)
                    {
                       color_attrib = readbyte(0x5800 + (calc_y / 8) * 32 + ff); // get 1 of 768 attrib values
                       pixel_map = readbyte(byte_offset + 0x4000);
                    }
                    else
                    {
                        color_attrib = Mem::ram7[0x1800 + (calc_y / 8) * 32 + ff]; // get 1 of 768 attrib values
                        pixel_map = Mem::ram7[byte_offset];
                    }

                    for (i = 0; i < 8; i++) // foreach pixel within a byte
                    {

                        zx_vidcalc = ff * 8 + i;
                        byte bitpos = (0x80 >> i);
                        bright = (color_attrib & 0B01000000) >> 6;
                        flash = (color_attrib & 0B10000000) >> 7;
                        zx_fore_color = zxColor((color_attrib & 0B00000111), bright);
                        zx_back_color = zxColor(((color_attrib & 0B00111000) >> 3), bright);

                        if (flash && flashing)
                            swap_flash(&zx_fore_color, &zx_back_color);

                        if ((pixel_map & bitpos) != 0)
                            vga.dotFast(zx_vidcalc + OFF_X+BOR_W, calc_y + OFF_Y+BOR_H, zx_fore_color);

                        else
                            vga.dotFast(zx_vidcalc + OFF_X+BOR_W, calc_y + OFF_Y+BOR_H, zx_back_color);
                    }
                }
            }
        }

        xQueueReceive(vidQueue, &param, portMAX_DELAY);
        videoTaskIsRunning = false;
    }
    videoTaskIsRunning = false;
    vTaskDelete(NULL);

    while (1) {
    }
}

void ESPectrum::waitForVideoTask() {
    xQueueSend(vidQueue, &param, portMAX_DELAY);
    // Wait while ULA loop is finishing
    delay(45);
}


void swap_flash(word *a, word *b) {
    word temp = *a;
    *a = *b;
    *b = temp;
}

// SPECTRUM SCREEN DISPLAY
//
/* Calculate Y coordinate (0-192) from Spectrum screen memory location */
static int calcY(int offset) {
    return ((offset >> 11) << 6)                                            // sector start
           + ((offset % 2048) >> 8)                                         // pixel rows
           + ((((offset % 2048) >> 5) - ((offset % 2048) >> 8 << 3)) << 3); // character rows
}

/* Calculate X coordinate (0-255) from Spectrum screen memory location */
static int calcX(int offset) { return (offset % 32) << 3; }

static word vga_colors[16] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

uint16_t ESPectrum::zxColor(uint8_t color, uint8_t bright) {
    if (bright) color += 8;
    return vga_colors[color];
}

/* Load zx keyboard lines from PS/2 */
void ESPectrum::processKeyboard() {
    byte kempston = 0;

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
    Ports::base[0x1f] = 0;
    bitWrite(Ports::base[0x1f], 0, !PS2Keyboard::keymap[KEY_CURSOR_RIGHT]);
    bitWrite(Ports::base[0x1f], 1, !PS2Keyboard::keymap[KEY_CURSOR_LEFT]);
    bitWrite(Ports::base[0x1f], 2, !PS2Keyboard::keymap[KEY_CURSOR_DOWN]);
    bitWrite(Ports::base[0x1f], 3, !PS2Keyboard::keymap[KEY_CURSOR_UP]);
    bitWrite(Ports::base[0x1f], 4, !PS2Keyboard::keymap[KEY_ALT_GR]);
}

/* +-------------+
   | LOOP core 1 |
   +-------------+
 */
void ESPectrum::loop() {
    if (halfsec) {
        flashing = ~flashing;
    }
    sp_int_ctr++;
    halfsec = !(sp_int_ctr % 25);

    processKeyboard();
    updateWiimote2Keys();
    OSD::do_OSD();

    zx_loop();

    AySound::update();

    xQueueSend(vidQueue, &param, portMAX_DELAY);

    while (videoTaskIsRunning) {
    }

    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
    vTaskDelay(0); // important to avoid task watchdog timeouts - change this to slow down emu
}
