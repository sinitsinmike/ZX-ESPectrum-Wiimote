#include "hardconfig.h"
#include "FileUtils.h"
#include "PS2Kbd.h"
#include "z80main.h"
#include "ESPectrum.h"
#include "messages.h"
#include "osd.h"
#include <FS.h>
#include "Wiimote2Keys.h"
#include "Config.h"
#include "FileSNA.h"

#ifdef CPU_JLSANCHEZ
#include "Z80.h"

extern Z80 z80;
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

static uint16_t readWordLE(File f)
{
    uint8_t lo = f.read();
    uint8_t hi = f.read();
    return lo | (hi << 8);
}

static uint16_t readWordBE(File f)
{
    uint8_t hi = f.read();
    uint8_t lo = f.read();
    return lo | (hi << 8);
}

bool FileSNA::load(String sna_fn)
{
    File lhandle;
    uint16_t retaddr;
    int sna_size;
    zx_reset();

    Serial.printf("%s SNA: %ub\n", MSG_FREE_HEAP_BEFORE, ESP.getFreeHeap());

    KB_INT_STOP;

    if (sna_fn != DISK_PSNA_FILE)
        loadKeytableForGame(sna_fn.c_str());

    lhandle = FileUtils::safeOpenFileRead(sna_fn);
    sna_size = lhandle.size();

    if (sna_size < SNA_48K_SIZE) {
        Serial.printf("FileSNA::load: bad SNA %s: size < %d", sna_fn.c_str(), SNA_48K_SIZE);
        KB_INT_START;
        return false;
    }

    String fileArch = "48K";

    Mem::bankLatch = 0;
    Mem::pagingLock = 1;
    Mem::videoLatch = 0;

    // Read in the registers
#ifdef CPU_LINKEFONG
    _zxCpu.i = lhandle.read();

    _zxCpu.alternates[Z80_HL] = readWordLE(lhandle);
    _zxCpu.alternates[Z80_DE] = readWordLE(lhandle);
    _zxCpu.alternates[Z80_BC] = readWordLE(lhandle);
    _zxCpu.alternates[Z80_AF] = readWordLE(lhandle);

    _zxCpu.registers.word[Z80_HL] = readWordLE(lhandle);
    _zxCpu.registers.word[Z80_DE] = readWordLE(lhandle);
    _zxCpu.registers.word[Z80_BC] = readWordLE(lhandle);

    _zxCpu.registers.word[Z80_IY] = readWordLE(lhandle);
    _zxCpu.registers.word[Z80_IX] = readWordLE(lhandle);

    uint8_t inter = lhandle.read();
    _zxCpu.iff2 = (inter & 0x04) ? 1 : 0;
    _zxCpu.iff1 = _zxCpu.iff2;
    _zxCpu.r = lhandle.read();

    _zxCpu.registers.word[Z80_AF] = readWordLE(lhandle);
    _zxCpu.registers.word[Z80_SP] = readWordLE(lhandle);

    _zxCpu.im = lhandle.read();
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
    z80.setRegI(lhandle.read());

    z80.setRegHLx(readWordLE(lhandle));
    z80.setRegDEx(readWordLE(lhandle));
    z80.setRegBCx(readWordLE(lhandle));
    z80.setRegAFx(readWordLE(lhandle));

    z80.setRegHL(readWordLE(lhandle));
    z80.setRegDE(readWordLE(lhandle));
    z80.setRegBC(readWordLE(lhandle));

    z80.setRegIY(readWordLE(lhandle));
    z80.setRegIX(readWordLE(lhandle));

    uint8_t inter = lhandle.read();
    z80.setIFF2((inter & 0x04) ? true : false);
    z80.setIFF1(z80.isIFF2());
    z80.setRegR(lhandle.read());

    z80.setRegAF(readWordLE(lhandle));
    z80.setRegSP(readWordLE(lhandle));

    z80.setIM((Z80::IntMode)lhandle.read());
#endif  // CPU_JLSANCHEZ

    byte bordercol = lhandle.read();
    ESPectrum::borderColor = bordercol;

    if (sna_size == SNA_48K_SIZE)
    {
        fileArch = "48K";

        uint16_t offset = 0x4000;
        while (lhandle.available()) {
            writebyte(offset, lhandle.read());
            offset++;
        }

#ifdef CPU_LINKEFONG
        uint16_t SP = _zxCpu.registers.word[Z80_SP];
        retaddr = readword(SP);
        _zxCpu.pc = retaddr;
        _zxCpu.registers.word[Z80_SP] = SP + 2;
#endif // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ
        uint16_t SP = z80.getRegSP();
        retaddr = readword(SP);
        z80.setRegPC(retaddr);
        z80.setRegSP(SP + 2);
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
    KB_INT_STOP;

    // only 48K snapshot supported at the moment
    if (Config::getArch() != "48K") {
        Serial.println("FileSNA::save: only 48K supported at the moment");
        KB_INT_START;
        return false;
    }

    // open file
    File f = THE_FS.open(sna_file, FILE_WRITE);
    if (!f) {
        Serial.printf("FileSNA::save: failed to open %s for writing\n", sna_file.c_str());
        KB_INT_START;
        return false;
    }

    // write registers: begin with I
    f.write(_zxCpu.i);

    // store registers
    unsigned short HL = _zxCpu.registers.word[Z80_HL];
    unsigned short DE = _zxCpu.registers.word[Z80_DE];
    unsigned short BC = _zxCpu.registers.word[Z80_BC];
    unsigned short AF = _zxCpu.registers.word[Z80_AF];

    // put alternates in registers
    _zxCpu.registers.word[Z80_HL] = _zxCpu.alternates[Z80_HL];
    _zxCpu.registers.word[Z80_DE] = _zxCpu.alternates[Z80_DE];
    _zxCpu.registers.word[Z80_BC] = _zxCpu.alternates[Z80_BC];
    _zxCpu.registers.word[Z80_AF] = _zxCpu.alternates[Z80_AF];

    // write alternates
    f.write(_zxCpu.registers.byte[Z80_L]);
    f.write(_zxCpu.registers.byte[Z80_H]);
    f.write(_zxCpu.registers.byte[Z80_E]);
    f.write(_zxCpu.registers.byte[Z80_D]);
    f.write(_zxCpu.registers.byte[Z80_C]);
    f.write(_zxCpu.registers.byte[Z80_B]);
    f.write(_zxCpu.registers.byte[Z80_F]);
    f.write(_zxCpu.registers.byte[Z80_A]);

    // restore registers
    _zxCpu.registers.word[Z80_HL] = HL;
    _zxCpu.registers.word[Z80_DE] = DE;
    _zxCpu.registers.word[Z80_BC] = BC;
    _zxCpu.registers.word[Z80_AF] = AF;

    // write registers
    f.write(_zxCpu.registers.byte[Z80_L]);
    f.write(_zxCpu.registers.byte[Z80_H]);
    f.write(_zxCpu.registers.byte[Z80_E]);
    f.write(_zxCpu.registers.byte[Z80_D]);
    f.write(_zxCpu.registers.byte[Z80_C]);
    f.write(_zxCpu.registers.byte[Z80_B]);
    f.write(_zxCpu.registers.byte[Z80_IYL]);
    f.write(_zxCpu.registers.byte[Z80_IYH]);
    f.write(_zxCpu.registers.byte[Z80_IXL]);
    f.write(_zxCpu.registers.byte[Z80_IXH]);

    byte inter = _zxCpu.iff2 ? 0x04 : 0;
    f.write(inter);
    f.write(_zxCpu.r);

    f.write(_zxCpu.registers.byte[Z80_F]);
    f.write(_zxCpu.registers.byte[Z80_A]);

    // read stack pointer and decrement it for pushing PC
    unsigned short SP = _zxCpu.registers.word[Z80_SP];
    SP -= 2;
    byte sp_l = SP & 0xFF;
    byte sp_h = SP >> 8;
    f.write(sp_l);
    f.write(sp_h);

    f.write(_zxCpu.im);
    byte bordercol = ESPectrum::borderColor;
    f.write(bordercol);

    // push PC to stack
    unsigned short PC = _zxCpu.pc;
    byte pc_l = PC & 0xFF;
    byte pc_h = PC >> 8;
    writebyte(SP+0, pc_l);
    writebyte(SP+1, pc_h);

    // dump memory to file
    for (int addr = 0x4000; addr <= 0xFFFF; addr++) {
        byte b = readbyte(addr);
        f.write(b);
    }

    f.close();
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

    // write registers: begin with I
    *snaptr++ = _zxCpu.i;

    // store registers
    unsigned short HL = _zxCpu.registers.word[Z80_HL];
    unsigned short DE = _zxCpu.registers.word[Z80_DE];
    unsigned short BC = _zxCpu.registers.word[Z80_BC];
    unsigned short AF = _zxCpu.registers.word[Z80_AF];

    // put alternates in registers
    _zxCpu.registers.word[Z80_HL] = _zxCpu.alternates[Z80_HL];
    _zxCpu.registers.word[Z80_DE] = _zxCpu.alternates[Z80_DE];
    _zxCpu.registers.word[Z80_BC] = _zxCpu.alternates[Z80_BC];
    _zxCpu.registers.word[Z80_AF] = _zxCpu.alternates[Z80_AF];

    // write alternates
    *snaptr++ = _zxCpu.registers.byte[Z80_L];
    *snaptr++ = _zxCpu.registers.byte[Z80_H];
    *snaptr++ = _zxCpu.registers.byte[Z80_E];
    *snaptr++ = _zxCpu.registers.byte[Z80_D];
    *snaptr++ = _zxCpu.registers.byte[Z80_C];
    *snaptr++ = _zxCpu.registers.byte[Z80_B];
    *snaptr++ = _zxCpu.registers.byte[Z80_F];
    *snaptr++ = _zxCpu.registers.byte[Z80_A];

    // restore registers
    _zxCpu.registers.word[Z80_HL] = HL;
    _zxCpu.registers.word[Z80_DE] = DE;
    _zxCpu.registers.word[Z80_BC] = BC;
    _zxCpu.registers.word[Z80_AF] = AF;

    // write registers
    *snaptr++ = _zxCpu.registers.byte[Z80_L];
    *snaptr++ = _zxCpu.registers.byte[Z80_H];
    *snaptr++ = _zxCpu.registers.byte[Z80_E];
    *snaptr++ = _zxCpu.registers.byte[Z80_D];
    *snaptr++ = _zxCpu.registers.byte[Z80_C];
    *snaptr++ = _zxCpu.registers.byte[Z80_B];
    *snaptr++ = _zxCpu.registers.byte[Z80_IYL];
    *snaptr++ = _zxCpu.registers.byte[Z80_IYH];
    *snaptr++ = _zxCpu.registers.byte[Z80_IXL];
    *snaptr++ = _zxCpu.registers.byte[Z80_IXH];

    byte inter = _zxCpu.iff2 ? 0x04 : 0;
    *snaptr++ = inter;
    *snaptr++ = _zxCpu.r;

    *snaptr++ = _zxCpu.registers.byte[Z80_F];
    *snaptr++ = _zxCpu.registers.byte[Z80_A];

    // read stack pointer and decrement it for pushing PC
    unsigned short SP = _zxCpu.registers.word[Z80_SP];
    SP -= 2;
    byte sp_l = SP & 0xFF;
    byte sp_h = SP >> 8;
    *snaptr++ = sp_l;
    *snaptr++ = sp_h;

    *snaptr++ = _zxCpu.im;
    byte bordercol = ESPectrum::borderColor;
    *snaptr++ = bordercol;

    // push PC to stack
    unsigned short PC = _zxCpu.pc;
    byte pc_l = PC & 0xFF;
    byte pc_h = PC >> 8;
    writebyte(SP+0, pc_l);
    writebyte(SP+1, pc_h);

    // dump memory to file
    for (int addr = 0x4000; addr <= 0xFFFF; addr++) {
        byte b = readbyte(addr);
        *snaptr++ = b;
    }

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

    byte* snaptr = snabuf;

    // Read in the registers
    _zxCpu.i = *snaptr++;
    _zxCpu.registers.byte[Z80_L] = *snaptr++;
    _zxCpu.registers.byte[Z80_H] = *snaptr++;
    _zxCpu.registers.byte[Z80_E] = *snaptr++;
    _zxCpu.registers.byte[Z80_D] = *snaptr++;
    _zxCpu.registers.byte[Z80_C] = *snaptr++;
    _zxCpu.registers.byte[Z80_B] = *snaptr++;
    _zxCpu.registers.byte[Z80_F] = *snaptr++;
    _zxCpu.registers.byte[Z80_A] = *snaptr++;

    _zxCpu.alternates[Z80_HL] = _zxCpu.registers.word[Z80_HL];
    _zxCpu.alternates[Z80_DE] = _zxCpu.registers.word[Z80_DE];
    _zxCpu.alternates[Z80_BC] = _zxCpu.registers.word[Z80_BC];
    _zxCpu.alternates[Z80_AF] = _zxCpu.registers.word[Z80_AF];

    _zxCpu.registers.byte[Z80_L] = *snaptr++;
    _zxCpu.registers.byte[Z80_H] = *snaptr++;
    _zxCpu.registers.byte[Z80_E] = *snaptr++;
    _zxCpu.registers.byte[Z80_D] = *snaptr++;
    _zxCpu.registers.byte[Z80_C] = *snaptr++;
    _zxCpu.registers.byte[Z80_B] = *snaptr++;
    _zxCpu.registers.byte[Z80_IYL] = *snaptr++;
    _zxCpu.registers.byte[Z80_IYH] = *snaptr++;
    _zxCpu.registers.byte[Z80_IXL] = *snaptr++;
    _zxCpu.registers.byte[Z80_IXH] = *snaptr++;

    byte inter = *snaptr++;
    _zxCpu.iff2 = (inter & 0x04) ? 1 : 0;
    _zxCpu.r = *snaptr++;

    _zxCpu.registers.byte[Z80_F] = *snaptr++;
    _zxCpu.registers.byte[Z80_A] = *snaptr++;

    byte sp_l = *snaptr++;
    byte sp_h = *snaptr++;
    _zxCpu.registers.word[Z80_SP] = sp_l + sp_h * 0x100;

    _zxCpu.im = *snaptr++;
    byte bordercol = *snaptr++;

    ESPectrum::borderColor = bordercol;

    _zxCpu.iff1 = _zxCpu.iff2;

    uint16_t thestack = _zxCpu.registers.word[Z80_SP];
    for (int addr = 0x4000; addr <= 0xFFFF; addr++) {
        writebyte(addr, *snaptr++);
    }

    uint16_t retaddr = readword(thestack);
    Serial.printf("%x\n", retaddr);
    _zxCpu.registers.word[Z80_SP]++;
    _zxCpu.registers.word[Z80_SP]++;

    _zxCpu.pc = retaddr;

    KB_INT_START;
    return true;
}