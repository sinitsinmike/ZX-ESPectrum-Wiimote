#include "osd.h"
#include "FileUtils.h"
#include "PS2Kbd.h"
#include "z80main.h"
#include "ESPectrum.h"
#include "messages.h"
#include "Wiimote2Keys.h"
#include "Config.h"
#include "FileSNA.h"

#define MENU_REDRAW true
#define MENU_UPDATE false
#define OSD_ERROR true
#define OSD_NORMAL false

#ifdef AR_16_9
#define SCR_W 360
#define SCR_H 200
#endif

#ifdef AR_4_3
#define SCR_W 320
#define SCR_H 240
#endif

#define OSD_W 248
#define OSD_H 152
#define OSD_MARGIN 4

#define LEVEL_INFO 0
#define LEVEL_OK 1
#define LEVEL_WARN 2
#define LEVEL_ERROR 3

extern Font Font6x8;

// X origin to center an element with pixel_width
unsigned short OSD::scrAlignCenterX(unsigned short pixel_width) { return (SCR_W / 2) - (pixel_width / 2); }

// Y origin to center an element with pixel_height
unsigned short OSD::scrAlignCenterY(unsigned short pixel_height) { return (SCR_H / 2) - (pixel_height / 2); }

byte OSD::osdMaxRows() { return (OSD_H - (OSD_MARGIN * 2)) / OSD_FONT_H; }
byte OSD::osdMaxCols() { return (OSD_W - (OSD_MARGIN * 2)) / OSD_FONT_W; }
unsigned short OSD::osdInsideX() { return scrAlignCenterX(OSD_W) + OSD_MARGIN; }
unsigned short OSD::osdInsideY() { return scrAlignCenterY(OSD_H) + OSD_MARGIN; }

// Cursor to OSD first row,col
void OSD::osdHome() { ESPectrum::vga.setCursor(osdInsideX(), osdInsideY()); }

// Cursor positioning
void OSD::osdAt(byte row, byte col) {
    if (row > osdMaxRows() - 1)
        row = 0;
    if (col > osdMaxCols() - 1)
        col = 0;
    unsigned short y = (row * OSD_FONT_H) + osdInsideY();
    unsigned short x = (col * OSD_FONT_W) + osdInsideX();
    ESPectrum::vga.setCursor(x, y);
}

void OSD::drawOSD() {
    VGA& vga = ESPectrum::vga;
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);
    vga.fillRect(x, y, OSD_W, OSD_H, ESPectrum::zxColor(1, 0));
    vga.rect(x, y, OSD_W, OSD_H, ESPectrum::zxColor(0, 0));
    vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, ESPectrum::zxColor(7, 0));
    vga.setTextColor(ESPectrum::zxColor(0, 0), ESPectrum::zxColor(5, 1));
    vga.setFont(Font6x8);
    osdHome();
    vga.print(OSD_TITLE);
    osdAt(17, 0);
    vga.print(OSD_BOTTOM);
    osdHome();
}

static void quickSave()
{
    OSD::osdCenteredMsg(OSD_QSNA_SAVING, LEVEL_INFO);
    if (!FileSNA::saveQuick()) {
        OSD::osdCenteredMsg(OSD_QSNA_SAVE_ERR, LEVEL_WARN);
        delay(1000);
        return;
    }
    OSD::osdCenteredMsg(OSD_QSNA_SAVED, LEVEL_INFO);
    delay(200);
}

static void quickLoad()
{
    if (!FileSNA::isQuickAvailable()) {
        OSD::osdCenteredMsg(OSD_QSNA_NOT_AVAIL, LEVEL_INFO);
        delay(1000);
        return;
    }
    OSD::osdCenteredMsg(OSD_QSNA_LOADING, LEVEL_INFO);
    if (!FileSNA::loadQuick()) {
        OSD::osdCenteredMsg(OSD_QSNA_LOAD_ERR, LEVEL_WARN);
        delay(1000);
        return;
    }
    OSD::osdCenteredMsg(OSD_QSNA_LOADED, LEVEL_INFO);
    delay(200);
}

static void persistSave()
{
    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO);
    if (!FileSNA::save(DISK_PSNA_FILE)) {
        OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
        delay(1000);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_SAVED, LEVEL_INFO);
    delay(400);
}

static void persistLoad()
{
    if (!FileSNA::isPersistAvailable()) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        delay(1000);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_LOADING, LEVEL_INFO);
    FileSNA::load(DISK_PSNA_FILE);
    // if (!FileSNA::load(DISK_PSNA_FILE)) {
    //     osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
    //     delay(1000);
    // }
    OSD::osdCenteredMsg(OSD_PSNA_LOADED, LEVEL_INFO);
    delay(400);
}

// OSD Main Loop
void OSD::do_OSD() {
    VGA& vga = ESPectrum::vga;
    static byte last_sna_row = 0;
    static unsigned int last_demo_ts = millis() / 1000;
    boolean cycle_sna = false;
    if (PS2Keyboard::checkAndCleanKey(KEY_F12)) {
        cycle_sna = true;
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_PAUSE)) {
        osdCenteredMsg(OSD_PAUSE, LEVEL_INFO);
        while (!PS2Keyboard::checkAndCleanKey(KEY_PAUSE)) {
            delay(5);
        }
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_F2)) {
        quickSave();
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_F3)) {
        quickLoad();
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_F4)) {
        persistSave();
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_F5)) {
        persistLoad();
    }
    else if (PS2Keyboard::checkAndCleanKey(KEY_F1)) {
        // Main menu
        byte opt = menuRun(MENU_MAIN);
        if (opt == 1) {
            // Change RAM
            unsigned short snanum = menuRun(Config::sna_name_list);
            if (snanum > 0) {
                changeSnapshot(rowGet(Config::sna_file_list, snanum));
            }
        }
        else if (opt == 2) {
            // Change ROM
            String arch_menu = getArchMenu();
            byte arch_num = menuRun(arch_menu);
            if (arch_num > 0) {
                String romset_menu = getRomsetMenu(rowGet(arch_menu, arch_num));
                byte romset_num = menuRun(romset_menu);
                if (romset_num > 0) {
                    String arch = rowGet(arch_menu, arch_num);
                    String romSet = rowGet(romset_menu, romset_num);
                    Config::requestMachine(arch, romSet, true);
                    vTaskDelay(2);

                    Config::save();
                    vTaskDelay(2);
                    zx_reset();
                }
            }
        }
        else if (opt == 3) {
            quickSave();
        }
        else if (opt == 4) {
            quickLoad();
        }
        else if (opt == 5) {
            persistSave();
        }
        else if (opt == 6) {
            persistLoad();
        }
        else if (opt == 7) {
            // Reset
            byte opt2 = menuRun(MENU_RESET);
            if (opt2 == 1) {
                // Soft
                zx_reset();
                if (Config::ram_file != (String)NO_RAM_FILE)
                    FileSNA::load("/sna/" + Config::ram_file);
            }
            else if (opt2 == 2) {
                // Hard
                Config::ram_file = (String)NO_RAM_FILE;
                Config::save();
                zx_reset();
                ESP.restart();
            }
            else if (opt2 == 3) {
                // ESP host reset
                ESP.restart();
            }
        }
        else if (opt == 8) {
            // Help
            drawOSD();
            osdAt(2, 0);
            vga.setTextColor(ESPectrum::zxColor(7, 0), ESPectrum::zxColor(1, 0));
            vga.print(OSD_HELP);
            while (!PS2Keyboard::checkAndCleanKey(KEY_F1) &&
                   !PS2Keyboard::checkAndCleanKey(KEY_ESC) &&
                   !PS2Keyboard::checkAndCleanKey(KEY_ENTER)) {
                vTaskDelay(5);
                updateWiimote2KeysOSD();
            }
        }
        // Exit
    }
}

// Shows a red panel with error text
void OSD::errorPanel(String errormsg) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);

    if (Config::slog_on)
        Serial.println(errormsg);

    ESPectrum::waitForVideoTask();
    VGA& vga = ESPectrum::vga;

    vga.fillRect(x, y, OSD_W, OSD_H, ESPectrum::zxColor(0, 0));
    vga.rect(x, y, OSD_W, OSD_H, ESPectrum::zxColor(7, 0));
    vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, ESPectrum::zxColor(2, 1));
    vga.setFont(Font6x8);
    osdHome();
    vga.setTextColor(ESPectrum::zxColor(7, 1), ESPectrum::zxColor(2, 1));
    vga.print(ERROR_TITLE);
    osdAt(2, 0);
    vga.setTextColor(ESPectrum::zxColor(7, 1), ESPectrum::zxColor(0, 0));
    vga.println(errormsg.c_str());
    osdAt(17, 0);
    vga.setTextColor(ESPectrum::zxColor(7, 1), ESPectrum::zxColor(2, 1));
    vga.print(ERROR_BOTTOM);
}

// Error panel and infinite loop
void OSD::errorHalt(String errormsg) {
    errorPanel(errormsg);
    while (1) {
        ESPectrum::processKeyboard();
        do_OSD();
        delay(5);
    }
}

// Centered message
void OSD::osdCenteredMsg(String msg, byte warn_level) {
    const unsigned short w = (msg.length() + 2) * OSD_FONT_W;
    const unsigned short h = OSD_FONT_H * 3;
    const unsigned short x = scrAlignCenterX(w);
    const unsigned short y = scrAlignCenterY(h);
    unsigned short paper;
    unsigned short ink;

    switch (warn_level) {
    case LEVEL_OK:
        ink = ESPectrum::zxColor(7, 1);
        paper = ESPectrum::zxColor(4, 0);
        break;
    case LEVEL_ERROR:
        ink = ESPectrum::zxColor(7, 1);
        paper = ESPectrum::zxColor(2, 0);
        break;
    case LEVEL_WARN:
        ink = ESPectrum::zxColor(0, 0);
        paper = ESPectrum::zxColor(6, 0);
        break;
    default:
        ink = ESPectrum::zxColor(7, 0);
        paper = ESPectrum::zxColor(1, 0);
    }

    ESPectrum::waitForVideoTask();
    VGA& vga = ESPectrum::vga;

    vga.fillRect(x, y, w, h, paper);
    // vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    vga.setTextColor(ink, paper);
    vga.setFont(Font6x8);
    vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    vga.print(msg.c_str());
}

// Count NL chars inside a string, useful to count menu rows
unsigned short OSD::rowCount(String menu) {
    unsigned short count = 0;
    for (unsigned short i = 1; i < menu.length(); i++) {
        if (menu.charAt(i) == ASCII_NL) {
            count++;
        }
    }
    return count;
}

// Get a row text
String OSD::rowGet(String menu, unsigned short row) {
    unsigned short count = 0;
    unsigned short last = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if (menu.charAt(i) == ASCII_NL) {
            if (count == row) {
                return menu.substring(last, i);
            }
            count++;
            last = i + 1;
        }
    }
    return "MENU ERROR! (Unknown row?)";
}

#include "FileSNA.h"
#include "FileZ80.h"

// Change running snapshot
void OSD::changeSnapshot(String filename)
{
    if (FileUtils::hasSNAextension(filename))
    {
        osdCenteredMsg((String)MSG_LOADING_SNA + ": " + filename, LEVEL_INFO);
        zx_reset();
        Serial.printf("Loading SNA: %s\n", filename.c_str());
        FileSNA::load((String)DISK_SNA_DIR + "/" + filename);
    }
    else if (FileUtils::hasZ80extension(filename))
    {
        osdCenteredMsg((String)MSG_LOADING_Z80 + ": " + filename, LEVEL_INFO);
        zx_reset();
        Serial.printf("Loading Z80: %s\n", filename.c_str());
        FileZ80::load((String)DISK_SNA_DIR + "/" + filename);
    }
    osdCenteredMsg(MSG_SAVE_CONFIG, LEVEL_WARN);
    Config::ram_file = filename;
    Config::save();
}

