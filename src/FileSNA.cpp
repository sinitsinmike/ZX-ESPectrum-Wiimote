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
#include "FileUtils.h"
#include "PS2Kbd.h"
#include "CPU.h"
#include "Mem.h"
#include "ESPectrum.h"
#include "messages.h"
#include "osd.h"
#include <FS.h>
#include "Wiimote2Keys.h"
#include "Config.h"
#include "FileSNA.h"

#ifdef CPU_LINKEFONG
#include "Z80_LKF/z80emu.h"
extern Z80_STATE _zxCpu;
#endif

#ifdef CPU_JLSANCHEZ
#include "Z80_JLS/z80.h"
#endif

#ifdef USE_INT_FLASH
// using internal storage (spi flash)
#include <SPIFFS.h>
// set The Filesystem to SPIFFS
#define THE_FS SPIFFS
#endif

#ifdef USE_SD_CARD
// using external storage (SD card)
#include <SD.h>
// set The Filesystem to SD
#define THE_FS SD
#endif



bool FileSNA::load(String sna_fn)
{
    File lhandle;
    uint16_t retaddr;
    int sna_size;
    ESPectrum::reset();

    Serial.printf("%s SNA: %ub\n", MSG_FREE_HEAP_BEFORE, ESP.getFreeHeap());

    KB_INT_STOP;

    if (sna_fn != DISK_PSNA_FILE)
        loadKeytableForGame(sna_fn.c_str());

    lhandle = FileUtils::safeOpenFileRead(sna_fn);
    sna_size = lhandle.size();

    if (sna_size < SNA_48K_SIZE) {
        Serial.printf("FileSNA::load: bad SNA %s: size = %d < %d\n", sna_fn.c_str(), sna_size, SNA_48K_SIZE);
        KB_INT_START;
        return false;
    }

    String fileArch = "48K";

    Mem::bankLatch = 0;
    Mem::pagingLock = 1;
    Mem::videoLatch = 0;

    // Read in the registers
#ifdef CPU_LINKEFONG
    _zxCpu.i = readByteFile(lhandle);

    _zxCpu.alternates[Z80_HL] = readWordFileLE(lhandle);
    _zxCpu.alternates[Z80_DE] = readWordFileLE(lhandle);
    _zxCpu.alternates[Z80_BC] = readWordFileLE(lhandle);
    _zxCpu.alternates[Z80_AF] = readWordFileLE(lhandle);

    _zxCpu.registers.word[Z80_HL] = readWordFileLE(lhandle);
    _zxCpu.registers.word[Z80_DE] = readWordFileLE(lhandle);
    _zxCpu.registers.word[Z80_BC] = readWordFileLE(lhandle);

    _zxCpu.registers.word[Z80_IY] = readWordFileLE(lhandle);
    _zxCpu.registers.word[Z80_IX] = readWordFileLE(lhandle);

    uint8_t inter = readByteFile(lhandle);
    _zxCpu.iff2 = (inter & 0x04) ? 1 : 0;
    _zxCpu.iff1 = _zxCpu.iff2;
    _zxCpu.r = readByteFile(lhandle);

    _zxCpu.registers.word[Z80_AF] = readWordFileLE(lhandle);
    _zxCpu.registers.word[Z80_SP] = readWordFileLE(lhandle);

    _zxCpu.im = readByteFile(lhandle);
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
    Z80::setRegI(readByteFile(lhandle));

    Z80::setRegHLx(readWordFileLE(lhandle));
    Z80::setRegDEx(readWordFileLE(lhandle));
    Z80::setRegBCx(readWordFileLE(lhandle));
    Z80::setRegAFx(readWordFileLE(lhandle));

    Z80::setRegHL(readWordFileLE(lhandle));
    Z80::setRegDE(readWordFileLE(lhandle));
    Z80::setRegBC(readWordFileLE(lhandle));

    Z80::setRegIY(readWordFileLE(lhandle));
    Z80::setRegIX(readWordFileLE(lhandle));

    uint8_t inter = readByteFile(lhandle);
    Z80::setIFF2((inter & 0x04) ? true : false);
    Z80::setIFF1(Z80::isIFF2());
    Z80::setRegR(readByteFile(lhandle));

    Z80::setRegAF(readWordFileLE(lhandle));
    Z80::setRegSP(readWordFileLE(lhandle));

    Z80::setIM((Z80::IntMode)readByteFile(lhandle));
#endif  // CPU_JLSANCHEZ

    ESPectrum::borderColor = readByteFile(lhandle);

    if (sna_size == SNA_48K_SIZE)
    {
        fileArch = "48K";

        lhandle.read(Mem::ram5, 0x4000);
        lhandle.read(Mem::ram2, 0x4000);
        lhandle.read(Mem::ram0, 0x4000);

#ifdef CPU_LINKEFONG
        uint16_t SP = _zxCpu.registers.word[Z80_SP];
        _zxCpu.pc = Mem::readword(SP);
        _zxCpu.registers.word[Z80_SP] = SP + 2;
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
        uint16_t SP = Z80::getRegSP();
        Z80::setRegPC(Mem::readword(SP));
        Z80::setRegSP(SP + 2);
#endif  // CPU_JLSANCHEZ
    }
    else
    {
        // TBD        
        // fileArch = "128K";

        // uint16_t buf_p;
        // for (buf_p = 0x4000; buf_p < 0x8000; buf_p++) {
        //     writebyte(buf_p, lhandle.read());
        // }
        // for (buf_p = 0x8000; buf_p < 0xc000; buf_p++) {
        //     writebyte(buf_p, lhandle.read());
        // }

        // for (buf_p = 0xc000; buf_p < 0xffff; buf_p++) {
        //     writebyte(buf_p, lhandle.read());
        // }

        // byte machine_b = lhandle.read();
        // Serial.printf("Machine: %x\n", machine_b);
        // byte retaddr_l = lhandle.read();
        // byte retaddr_h = lhandle.read();
        // retaddr = retaddr_l + retaddr_h * 0x100;
        // byte tmp_port = lhandle.read();

        // byte tmp_byte;
        // for (int a = 0xc000; a < 0xffff; a++) {
        //     Mem::bankLatch = 0;
        //     tmp_byte = readbyte(a);
        //     Mem::bankLatch = tmp_port & 0x07;
        //     writebyte(a, tmp_byte);
        // }

        // byte tr_dos = lhandle.read();
        // byte tmp_latch = tmp_port & 0x7;
        // for (int page = 0; page < 8; page++) {
        //     if (page != tmp_latch && page != 2 && page != 5) {
        //         Mem::bankLatch = page;
        //         Serial.printf("Page %d actual_latch: %d\n", page, Mem::bankLatch);
        //         for (buf_p = 0xc000; buf_p <= 0xFFFF; buf_p++) {
        //             writebyte(buf_p, lhandle.read());
        //         }
        //     }
        // }

        // Mem::videoLatch = bitRead(tmp_port, 3);
        // Mem::romLatch = bitRead(tmp_port, 4);
        // Mem::pagingLock = bitRead(tmp_port, 5);
        // Mem::bankLatch = tmp_latch;
        // Mem::romInUse = Mem::romLatch;

        // _zxCpu.pc = retaddr;
    }
    lhandle.close();

    // just architecturey things
    if (Config::getArch() == "128K")
    {
        if (fileArch == "48K")
        {
#ifdef SNAPSHOT_LOAD_FORCE_ARCH
            Config::requestMachine("48K", "SINCLAIR", true);
            Mem::romInUse = 0;
#else
            Mem::romInUse = 1;
#endif
        }
    }
    else if (Config::getArch() == "48K")
    {
        if (fileArch == "128K")
        {
            Config::requestMachine("128K", "SINCLAIR", true);
            Mem::romInUse = 1;
        }
    }

    // Serial.printf("%s SNA: %u\n", MSG_FREE_HEAP_AFTER, ESP.getFreeHeap());
    // Serial.printf("Ret address: %x Stack: %x AF: %x Border: %x sna_size: %d rom: %d bank: %x\n", retaddr,
    //               _zxCpu.registers.word[Z80_SP], _zxCpu.registers.word[Z80_AF], ESPectrum::borderColor, sna_size, Mem::romInUse,
    //               Mem::bankLatch);

    KB_INT_START;
}

bool FileSNA::isPersistAvailable()
{
    String filename = DISK_PSNA_FILE;
    return THE_FS.exists(filename.c_str());
}

bool FileSNA::save(String sna_file) {
    // try to save using pages
    if (FileSNA::save(sna_file, true))
        return true;
    OSD::osdCenteredMsg(OSD_PSNA_SAVE_WARN, 2);
    // try to save byte-by-byte
    return FileSNA::save(sna_file, false);
}

bool FileSNA::save(String sna_file, bool writePages) {
    KB_INT_STOP;

    // only 48K snapshot supported at the moment
    if (Config::getArch() != "48K") {
        Serial.println("FileSNA::save: only 48K supported at the moment");
        KB_INT_START;
        return false;
    }

    // open file
    File file = THE_FS.open(sna_file, FILE_WRITE);
    if (!file) {
        Serial.printf("FileSNA::save: failed to open %s for writing\n", sna_file.c_str());
        KB_INT_START;
        return false;
    }

#ifdef CPU_LINKEFONG
    // write registers: begin with I
    writeByteFile(_zxCpu.i, file);

    writeWordFileLE(_zxCpu.alternates[Z80_HL], file);
    writeWordFileLE(_zxCpu.alternates[Z80_DE], file);
    writeWordFileLE(_zxCpu.alternates[Z80_BC], file);
    writeWordFileLE(_zxCpu.alternates[Z80_AF], file);

    writeWordFileLE(_zxCpu.registers.word[Z80_HL], file);
    writeWordFileLE(_zxCpu.registers.word[Z80_DE], file);
    writeWordFileLE(_zxCpu.registers.word[Z80_BC], file);

    writeWordFileLE(_zxCpu.registers.word[Z80_IY], file);
    writeWordFileLE(_zxCpu.registers.word[Z80_IX], file);

    byte inter = _zxCpu.iff2 ? 0x04 : 0;
    writeByteFile(inter, file);
    writeByteFile(_zxCpu.r, file);

    writeWordFileLE(_zxCpu.registers.word[Z80_AF], file);

    // read stack pointer and decrement it for pushing PC
    uint16_t SP = _zxCpu.registers.word[Z80_SP] - 2;
    writeWordFileLE(SP, file);

    writeByteFile(_zxCpu.im, file);
    byte bordercol = ESPectrum::borderColor;
    writeByteFile(bordercol, file);

    // push PC to stack, before dumping memory
    Mem::writeword(SP, _zxCpu.pc);

#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
    // write registers: begin with I
    writeByteFile(Z80::getRegI(), file);

    writeWordFileLE(Z80::getRegHLx(), file);
    writeWordFileLE(Z80::getRegDEx(), file);
    writeWordFileLE(Z80::getRegBCx(), file);
    writeWordFileLE(Z80::getRegAFx(), file);

    writeWordFileLE(Z80::getRegHL(), file);
    writeWordFileLE(Z80::getRegDE(), file);
    writeWordFileLE(Z80::getRegBC(), file);

    writeWordFileLE(Z80::getRegIY(), file);
    writeWordFileLE(Z80::getRegIX(), file);

    byte inter = Z80::isIFF2() ? 0x04 : 0;
    writeByteFile(inter, file);
    writeByteFile(Z80::getRegR(), file);

    writeWordFileLE(Z80::getRegAF(), file);

    // read stack pointer and decrement it for pushing PC
    uint16_t SP = Z80::getRegSP() - 2;
    writeWordFileLE(SP, file);

    writeByteFile(Z80::getIM(), file);
    byte bordercol = ESPectrum::borderColor;
    writeByteFile(bordercol, file);

    // push PC to stack, before dumping memory
    Mem::writeword(SP, Z80::getRegPC());
#endif  // CPU_JLSANCHEZ

#ifdef CPU_JLSANCHEZ
#endif  // CPU_JLSANCHEZ


    if (writePages) {
        // writing pages should be faster, but fails some times when flash is getting full.
        uint16_t pageSize = 0x4000;
        uint8_t pages[3] = {5, 2, 8};
        for (uint8_t ipage = 0; ipage < 3; ipage++) {
            uint8_t page = pages[ipage];
            uint16_t bytesWritten = file.write(Mem::ram[page], pageSize);
            //Serial.printf("page %d: %d bytes written\n", page, bytesWritten);
            if (bytesWritten != pageSize) {
                Serial.printf("error writing page %d: %d of %d bytes written\n", page, bytesWritten, pageSize);
                file.close();
                KB_INT_START;
                return false;
            }
        }
    }
    else {
        // dump memory to file
        for (int addr = 0x4000; addr <= 0xFFFF; addr++) {
            byte b = Mem::readbyte(addr);
            if (1 != file.write(b)) {
                Serial.printf("error writing byte from RAM address %d\n", addr);
                file.close();
                KB_INT_START;
                return false;
            }
        }
    }

    file.close();
    KB_INT_START;
    return true;
}

static byte* snabuf = NULL;

bool FileSNA::isQuickAvailable()
{
    return snabuf != NULL;
}

bool FileSNA::saveQuick()
{
    KB_INT_STOP;

    // only 48K snapshot supported at the moment
    if (Config::getArch() != "48K") {
        Serial.println("FileSNA::saveQuick: only 48K supported at the moment");
        KB_INT_START;
        return false;
    }

    // allocate buffer it not done yet
    if (snabuf == NULL)
    {
#ifdef BOARD_HAS_PSRAM
        snabuf = (byte*)ps_malloc(49179);
#else
        snabuf = (byte*)malloc(49179);
#endif
        if (snabuf == NULL) {
            Serial.println("FileSNA::saveQuick: cannot allocate memory for snapshot buffer");
            KB_INT_START;
            return false;
        }
    }

    byte* snaptr = snabuf;

#ifdef CPU_LINKEFONG
    // write registers: begin with I
    writeByteMem(_zxCpu.i, snaptr);

    writeWordMemLE(_zxCpu.alternates[Z80_HL], snaptr);
    writeWordMemLE(_zxCpu.alternates[Z80_DE], snaptr);
    writeWordMemLE(_zxCpu.alternates[Z80_BC], snaptr);
    writeWordMemLE(_zxCpu.alternates[Z80_AF], snaptr);

    writeWordMemLE(_zxCpu.registers.word[Z80_HL], snaptr);
    writeWordMemLE(_zxCpu.registers.word[Z80_DE], snaptr);
    writeWordMemLE(_zxCpu.registers.word[Z80_BC], snaptr);

    writeWordMemLE(_zxCpu.registers.word[Z80_IY], snaptr);
    writeWordMemLE(_zxCpu.registers.word[Z80_IX], snaptr);

    byte inter = _zxCpu.iff2 ? 0x04 : 0;
    writeByteMem(inter, snaptr);
    writeByteMem(_zxCpu.r, snaptr);

    writeWordMemLE(_zxCpu.registers.word[Z80_AF], snaptr);

    // read stack pointer and decrement it for pushing PC
    uint16_t SP = _zxCpu.registers.word[Z80_SP] - 2;
    writeWordMemLE(SP, snaptr);

    writeByteMem(_zxCpu.im, snaptr);
    byte bordercol = ESPectrum::borderColor;
    writeByteMem(bordercol, snaptr);

    // push PC to stack, before dumping memory
    Mem::writeword(SP, _zxCpu.pc);
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
    // write registers: begin with I
    writeByteMem(Z80::getRegI(), snaptr);

    writeWordMemLE(Z80::getRegHLx(), snaptr);
    writeWordMemLE(Z80::getRegDEx(), snaptr);
    writeWordMemLE(Z80::getRegBCx(), snaptr);
    writeWordMemLE(Z80::getRegAFx(), snaptr);

    writeWordMemLE(Z80::getRegHL(), snaptr);
    writeWordMemLE(Z80::getRegDE(), snaptr);
    writeWordMemLE(Z80::getRegBC(), snaptr);

    writeWordMemLE(Z80::getRegIY(), snaptr);
    writeWordMemLE(Z80::getRegIX(), snaptr);

    byte inter = Z80::isIFF2() ? 0x04 : 0;
    writeByteMem(inter, snaptr);
    writeByteMem(Z80::getRegR(), snaptr);

    writeWordMemLE(Z80::getRegAF(), snaptr);

    // read stack pointer and decrement it for pushing PC
    uint16_t SP = Z80::getRegSP() - 2;
    writeWordMemLE(SP, snaptr);

    writeByteMem(Z80::getIM(), snaptr);
    byte bordercol = ESPectrum::borderColor;
    writeByteMem(bordercol, snaptr);

    // push PC to stack, before dumping memory
    Mem::writeword(SP, Z80::getRegPC());
#endif  // CPU_JLSANCHEZ



    // dump memory to file
    uint16_t pageSize = 0x4000;
    memcpy(snaptr, Mem::ram[5], pageSize); snaptr += pageSize;
    memcpy(snaptr, Mem::ram[2], pageSize); snaptr += pageSize;
    memcpy(snaptr, Mem::ram[0], pageSize); snaptr += pageSize;

    KB_INT_START;
    return true;
}

bool FileSNA::loadQuick()
{
    // only 48K snapshot supported at the moment
    if (Config::getArch() != "48K") {
        Serial.println("FileSNA::loadQuick(): only 48K supported at the moment");
        KB_INT_START;
        return false;
    }

    if (NULL == snabuf) {
        // nothing to read
        Serial.println("FileSNA::loadQuick(): nothing to load");
        KB_INT_START;
        return false;
    }

    uint8_t* snaptr = snabuf;

#ifdef CPU_LINKEFONG
    _zxCpu.i = readByteMem(snaptr);

    _zxCpu.alternates[Z80_HL] = readWordMemLE(snaptr);
    _zxCpu.alternates[Z80_DE] = readWordMemLE(snaptr);
    _zxCpu.alternates[Z80_BC] = readWordMemLE(snaptr);
    _zxCpu.alternates[Z80_AF] = readWordMemLE(snaptr);

    _zxCpu.registers.word[Z80_HL] = readWordMemLE(snaptr);
    _zxCpu.registers.word[Z80_DE] = readWordMemLE(snaptr);
    _zxCpu.registers.word[Z80_BC] = readWordMemLE(snaptr);

    _zxCpu.registers.word[Z80_IY] = readWordMemLE(snaptr);
    _zxCpu.registers.word[Z80_IX] = readWordMemLE(snaptr);

    uint8_t inter = readByteMem(snaptr);
    _zxCpu.iff2 = (inter & 0x04) ? 1 : 0;
    _zxCpu.iff1 = _zxCpu.iff2;
    _zxCpu.r = readByteMem(snaptr);

    _zxCpu.registers.word[Z80_AF] = readWordMemLE(snaptr);
    _zxCpu.registers.word[Z80_SP] = readWordMemLE(snaptr);

    _zxCpu.im = readByteMem(snaptr);
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
    Z80::setRegI(readByteMem(snaptr));

    Z80::setRegHLx(readWordMemLE(snaptr));
    Z80::setRegDEx(readWordMemLE(snaptr));
    Z80::setRegBCx(readWordMemLE(snaptr));
    Z80::setRegAFx(readWordMemLE(snaptr));

    Z80::setRegHL(readWordMemLE(snaptr));
    Z80::setRegDE(readWordMemLE(snaptr));
    Z80::setRegBC(readWordMemLE(snaptr));

    Z80::setRegIY(readWordMemLE(snaptr));
    Z80::setRegIX(readWordMemLE(snaptr));

    uint8_t inter = readByteMem(snaptr);
    Z80::setIFF2((inter & 0x04) ? true : false);
    Z80::setIFF1(Z80::isIFF2());
    Z80::setRegR(readByteMem(snaptr));

    Z80::setRegAF(readWordMemLE(snaptr));
    Z80::setRegSP(readWordMemLE(snaptr));

    Z80::setIM((Z80::IntMode)readByteMem(snaptr));
#endif  // CPU_JLSANCHEZ

    ESPectrum::borderColor = readByteMem(snaptr);

    uint16_t pageSize = 0x4000;
    memcpy(Mem::ram[5], snaptr, pageSize); snaptr += pageSize;
    memcpy(Mem::ram[2], snaptr, pageSize); snaptr += pageSize;
    memcpy(Mem::ram[0], snaptr, pageSize); snaptr += pageSize;

#ifdef CPU_LINKEFONG
        uint16_t SP = _zxCpu.registers.word[Z80_SP];
        _zxCpu.pc = Mem::readword(SP);
        _zxCpu.registers.word[Z80_SP] = SP + 2;
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
        uint16_t SP = Z80::getRegSP();
        Z80::setRegPC(Mem::readword(SP));
        Z80::setRegSP(SP + 2);
#endif  // CPU_JLSANCHEZ

    KB_INT_START;
    return true;
}