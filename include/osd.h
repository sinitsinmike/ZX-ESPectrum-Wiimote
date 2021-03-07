#ifndef ESPECTRUM_OSD_H
#define ESPECTRUM_OSD_H

// OSD Headers
#include <Arduino.h>
#include "hardconfig.h"

// Defines
#define OSD_FONT_W 6
#define OSD_FONT_H 8

// OSD Interface
class OSD
{
public:
    // Calc
    static unsigned short scrAlignCenterX(unsigned short pixel_width);
    static unsigned short scrAlignCenterY(unsigned short pixel_height);
    static byte osdMaxRows();
    static byte osdMaxCols();
    static unsigned short osdInsideX();
    static unsigned short osdInsideY();

    // OSD
    static void osdHome();
    static void osdAt(byte row, byte col);
    static void drawOSD();
    static void do_OSD();

    // Error
    static void errorPanel(String errormsg);
    static void errorHalt(String errormsg);
    static void osdCenteredMsg(String msg, byte warn_level);

    // Menu
    static void newMenu(String new_menu);
    static void menuRecalc();
    static unsigned short menuRealRowFor(byte virtual_row_num);
    static void menuPrintRow(byte virtual_row_num, byte line_type);
    static void menuDraw();
    static void menuRedraw();
    static String getArchMenu();
    static String getRomsetMenu(String arch);
    static unsigned short menuRun(String new_menu);
    static void menuScroll(boolean up);
    static void menuAt(short int row, short int col);
    static void menuScrollBar();
    static String getTestMenu(unsigned short n_lines);

    // Rows
    static unsigned short rowCount(String menu);
    static String rowGet(String menu, unsigned short row_number);

    // Snapshot (SNA/Z80) Management
    static void changeSnapshot(String sna_filename);
};

#endif // ESPECTRUM_OSD_H
