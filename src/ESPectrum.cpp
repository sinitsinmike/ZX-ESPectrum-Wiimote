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

// works, but not needed for now
#pragma GCC optimize ("O3")

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
uint8_t* Mem::rom[4];

uint8_t* Mem::ram0 = NULL;
uint8_t* Mem::ram1 = NULL;
uint8_t* Mem::ram2 = NULL;
uint8_t* Mem::ram3 = NULL;
uint8_t* Mem::ram4 = NULL;
uint8_t* Mem::ram5 = NULL;
uint8_t* Mem::ram6 = NULL;
uint8_t* Mem::ram7 = NULL;
uint8_t* Mem::ram[8];

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

bool isLittleEndian()
{
	uint16_t val16 = 0x1;
	uint8_t* ptr8 = (uint8_t*)(&val16);
	return (*ptr8 == 1);
}

static uint8_t staticMemPage[0x4000];

static void tryAllocateSRamThenPSRam(uint8_t*& page, const char* pagename)
{
    page = (uint8_t*)malloc(0x4000);
    if (page != NULL) {
        Serial.printf("Page %s allocated into SRAM (fast)\n", pagename);
        return;
    }
    page = (uint8_t*)ps_malloc(0x4000);
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
    Serial.printf("HEAP BEGIN %d\n", ESP.getFreeHeap());

    initWiimote2Keys();

    Serial.printf("HEAP AFTER WIIMOTE %d\n", ESP.getFreeHeap());

    FileUtils::initFileSystem();
    Config::load();
    Config::loadSnapshotLists();

    Serial.printf("HEAP AFTER FILESYSTEM %d\n", ESP.getFreeHeap());

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

    // precalculate colors for current VGA mode
    precalcColors();

    vga.clear(0);

    Serial.printf("HEAP after vga  %d \n", ESP.getFreeHeap());

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

    Serial.printf("HEAP AFTER RAM %d\n", ESP.getFreeHeap());

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

#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))

void ESPectrum::videoTask(void *unused) {
    videoTaskIsRunning = true;
    uint16_t *param;

    while (1) {
        xQueueReceive(vidQueue, &param, portMAX_DELAY);
    
        int vgaX;   // from 0 to RESX - 1
        int vgaY;   // from 0 to RESY - 1
        int speX;   // from 0 to 256
        int speY;   // from 0 to 192
        int ulaX;   // from 0 to 32
        int ulaY;   // from 0 to 192, bit-swapped 76210543

        int bmpOffset;     // offset for bitmap in graphic memory
        int attOffset;     // offset for attrib in graphic memory

        int att, bmp;   // attribute and bitmap
        int fla, bri;   // flash and bright flags
        int pap, ink;   // paper and ink color
        int aux;        // auxiliary for flash swapping
        int back, fore; // background and foreground colors
        int pix;        // final pixel color

        uint8_t* grmem;

        uint32_t ts_start = micros();

        for (int vgaY = 0; vgaY < BOR_H+SPEC_H+BOR_H; vgaY++) {
            grmem = Mem::videoLatch ? Mem::ram7 : Mem::ram5;
            uint8_t* lineptr = vga.backBuffer[vgaY+OFF_Y];
            vgaX = OFF_X;
            if (vgaY < BOR_H || vgaY >= BOR_H + SPEC_H) {
                for (int i = 0; i < BOR_W+SPEC_W+BOR_W; i++, vgaX++) {
                    lineptr[vgaX^2] = zxColor(borderColor, 0);
                }
            }
            else
            {
                speY = vgaY - BOR_H;
                ulaY = ULA_SWAP(speY);
                lineptr = vga.backBuffer[speY+OFF_Y+BOR_H];

                vgaX = OFF_X;
                for (int i = 0; i < BOR_W; i++, vgaX++) {
                    lineptr[vgaX^2] = zxColor(borderColor, 0);
                }

                bmpOffset =   ulaY << 5;
                attOffset = ((speY >> 3) << 5) + 0x1800;

                for (ulaX = 0; ulaX < 32; ulaX++) // foreach byte in line
                {
                    att = grmem[attOffset + ulaX];  // get attribute byte

                    ink = (att     ) & 0b111;
                    pap = (att >> 3) & 0b111;
                    bri = (att >> 6) & 1;
                    fla = (att >> 7);
                    fore = zxColor(ink, bri);
                    back = zxColor(pap, bri);

                    if (fla && flashing) {
                        aux = fore; fore = back; back = aux;
                    }

                    bmp = grmem[bmpOffset + ulaX];  // get bitmap byte
                    for (int i = 0; i < 8; i++) // foreach pixel within a byte
                    {   
                        uint32_t mask = 0x80 >> i;
                        if (bmp & mask) pix = fore;
                        else            pix = back;
                        lineptr[vgaX^2] = pix;
                        vgaX++;
                    }
                }

                for (int i = 0; i < BOR_W; i++, vgaX++) {
                    lineptr[vgaX^2] = zxColor(borderColor, 0);
                }


            }
        }

        uint32_t ts_end = micros();

        uint32_t elapsed = ts_end - ts_start;
        uint32_t target = 19968;
        uint32_t idle = target - elapsed;
        if (idle < target)
            delayMicroseconds(idle);

//#define LOG_DEBUG_TIMING

#ifdef LOG_DEBUG_TIMING
        static int ctr = 0;
        if (ctr == 0) {
            ctr = 50;
            Serial.printf("[VideoTask] elapsed: %u; idle: %u\n", elapsed, idle);
        }
        else ctr--;
#endif
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

    xQueueSend(vidQueue, &param, portMAX_DELAY);
    uint32_t ts_start = micros();

    zx_loop();

    uint32_t ts_end = micros();

    uint32_t elapsed = ts_end - ts_start;
    uint32_t target = 19968;
    uint32_t idle = target - elapsed;
    // if (idle < target)
    //     delayMicroseconds(idle);

#ifdef LOG_DEBUG_TIMING
    static int ctr = 0;
    if (ctr == 0) {
        ctr = 50;
        Serial.printf("[CPUTask] elapsed: %u; idle: %u\n", elapsed, idle);
    }
    else ctr--;
#endif

    AySound::update();

    while (videoTaskIsRunning) {
    }

    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
    vTaskDelay(0); // important to avoid task watchdog timeouts - change this to slow down emu
}
