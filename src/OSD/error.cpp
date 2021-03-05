#include "Disk.h"
#include "ESPectrum.h"
#include "def/Font.h"
#include "def/files.h"
#include "def/msg.h"
#include "osd.h"

// Shows a red panel with error text
void errorPanel(String errormsg) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);

    if (cfg_slog_on)
        Serial.println(errormsg);

    ESPectrum::waitForVideoTask();

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
void errorHalt(String errormsg) {
    errorPanel(errormsg);
    while (1) {
        ESPectrum::processKeyboard();
        do_OSD();
        delay(5);
    }
}

// Centered message
void osdCenteredMsg(String msg, byte warn_level) {
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

    vga.fillRect(x, y, w, h, paper);
    // vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    vga.setTextColor(ink, paper);
    vga.setFont(Font6x8);
    vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    vga.print(msg.c_str());
}
