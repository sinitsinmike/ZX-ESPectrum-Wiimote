#include "hardpins.h"
#include <stdio.h>
#include <string.h>

#include "ESPectrum.h"

#include "PS2Kbd.h"
#include "z80main.h"
#include "Config.h"
#include "AySound.h"

#define RAM_AVAILABLE 0xC000

Z80_STATE _zxCpu;

extern byte tick;

CONTEXT _zxContext;
static uint16_t _attributeCount;
int _total;
int _next_total = 0;
static uint8_t zx_data = 0;
static uint32_t frames = 0;
static uint32_t _ticks = 0;

#define FRAME_PERIOD_MS 20
#define CPU_SPEED_MHZ_ZX48 3.5
#define CPU_SPEED_MHZ_ZX128 3.5469

int CalcTStates() {
    if (Config::getArch() == "48K") {
        return CPU_SPEED_MHZ_ZX48 * FRAME_PERIOD_MS * 1000;
    } else {
        return CPU_SPEED_MHZ_ZX128 * FRAME_PERIOD_MS * 1000;
    }
}

int cycles_per_step = CalcTStates();

extern "C" {
    uint8_t readbyte(uint16_t addr);
    uint16_t readword(uint16_t addr);
    void writebyte(uint16_t addr, uint8_t data);
    void writeword(uint16_t addr, uint16_t data);
    uint8_t input(uint8_t portLow, uint8_t portHigh);
    void output(uint8_t portLow, uint8_t portHigh, uint8_t data);
}

void zx_setup() {
    _zxContext.readbyte = readbyte;
    _zxContext.readword = readword;
    _zxContext.writeword = writeword;
    _zxContext.writebyte = writebyte;
    _zxContext.input = input;
    _zxContext.output = output;

    zx_reset();
}

void zx_reset() {
    for (uint8_t i = 0; i < 128; i++) {
        Ports::base[i] == 0x1F;
        Ports::wii[i] == 0x1F;
    }
    ESPectrum::borderColor = 7;
    Mem::bankLatch = 0;
    Mem::videoLatch = 0;
    Mem::romLatch = 0;
    Mem::pagingLock = 0;
    Mem::modeSP3 = 0;
    Mem::romSP3 = 0;
    Mem::romInUse = 0;
    cycles_per_step = CalcTStates();

    Z80Reset(&_zxCpu);
}

int32_t zx_loop() {
    int32_t result = -1;
    byte tmp_color = 0;

    _total = Z80Emulate(&_zxCpu, cycles_per_step, &_zxContext);
    Z80Interrupt(&_zxCpu, 0xff, &_zxContext);
  //  Serial.println(_total);

    return result;
}

extern "C" uint8_t readbyte(uint16_t addr) {
    switch (addr) {
    case 0x0000 ... 0x3fff:
        switch (Mem::romInUse) {
        case 0:
            return Mem::rom0[addr];
        case 1:
            return Mem::rom1[addr];
        case 2:
            return Mem::rom2[addr];
        case 3:
            return Mem::rom3[addr];
        }
    case 0x4000 ... 0x7fff:
        return Mem::ram5[addr - 0x4000];
        break;
    case 0x8000 ... 0xbfff:
        return Mem::ram2[addr - 0x8000];
        break;
    case 0xc000 ... 0xffff:
        switch (Mem::bankLatch) {
        case 0:
            return Mem::ram0[addr - 0xc000];
            break;
        case 1:
            return Mem::ram1[addr - 0xc000];
            break;
        case 2:
            return Mem::ram2[addr - 0xc000];
            break;
        case 3:
            return Mem::ram3[addr - 0xc000];
            break;
        case 4:
            return Mem::ram4[addr - 0xc000];
            break;
        case 5:
            return Mem::ram5[addr - 0xc000];
            break;
        case 6:
            return Mem::ram6[addr - 0xc000];
            break;
        case 7:
            return Mem::ram7[addr - 0xc000];
            break;
        }
        // Serial.printf("Address: %x Returned address %x  Bank: %x\n",addr,addr-0xc000,Mem::bankLatch);
        break;
    }
}

extern "C" uint16_t readword(uint16_t addr) { return ((readbyte(addr + 1) << 8) | readbyte(addr)); }

extern "C" void writebyte(uint16_t addr, uint8_t data)
{
    switch (addr) {
    case 0x0000 ... 0x3fff:
        return;
    case 0x4000 ... 0x7fff:
        Mem::ram5[addr - 0x4000] = data;
        break;
    case 0x8000 ... 0xbfff:
        Mem::ram2[addr - 0x8000] = data;
        break;
    case 0xc000 ... 0xffff:
        switch (Mem::bankLatch) {
        case 0:
            Mem::ram0[addr - 0xc000] = data;
            break;
        case 1:
            Mem::ram1[addr - 0xc000] = data;
            break;
        case 2:
            Mem::ram2[addr - 0xc000] = data;
            break;
        case 3:
            Mem::ram3[addr - 0xc000] = data;
            break;
        case 4:
            Mem::ram4[addr - 0xc000] = data;
            break;
        case 5:
            Mem::ram5[addr - 0xc000] = data;
            break;
        case 6:
            Mem::ram6[addr - 0xc000] = data;
            break;
        case 7:
            Mem::ram7[addr - 0xc000] = data;
            break;
        }
        // Serial.println("plin");
        break;
    }
    return;
}

extern "C" void writeword(uint16_t addr, uint16_t data) {
    writebyte(addr, (uint8_t)data);
    writebyte(addr + 1, (uint8_t)(data >> 8));
}

#ifdef ZX_KEYB_PRESENT
const int psKR[] = {AD8, AD9, AD10, AD11, AD12, AD13, AD14, AD15};
const int psKC[] = {DB0, DB1, DB2, DB3, DB4};
extern "C" void detectZXKeyCombinationForMenu()
{
    uint8_t portHigh;

    // try to detect caps shift - column 0xFE, bit 0
    portHigh = 0xFE;
    for (int b = 0; b < 8; b++)
        digitalWrite(psKR[b], ((portHigh >> b) & 0x1) ? HIGH : LOW);
    //delay(2);
    if (0 != digitalRead(DB0))
    return;

    // try to detect symbol shift - column 0x7F, bit 1
    portHigh = 0x7F;
    for (int b = 0; b < 8; b++)
        digitalWrite(psKR[b], ((portHigh >> b) & 0x1) ? HIGH : LOW);
    //delay(2);
    if (0 != digitalRead(DB1))
    return;

    // try to detect enter - column 0xBF, bit 0
    portHigh = 0xBF;
    for (int b = 0; b < 8; b++)
        digitalWrite(psKR[b], ((portHigh >> b) & 0x1) ? HIGH : LOW);
    //delay(2);
    if (0 != digitalRead(DB0))
    return;

    emulateKeyChange(KEY_F1, 1);
}
#endif // ZX_KEYB_PRESENT

extern "C" uint8_t input(uint8_t portLow, uint8_t portHigh)
{
    if (portLow == 0xFE) {
        uint8_t result = 0xFF;

        // EAR_PIN
        if (portHigh == 0xFE) {
#ifdef EAR_PRESENT
            bitWrite(result, 6, digitalRead(EAR_PIN));
#endif
        }

        // Keyboard
        if (~(portHigh | 0xFE)&0xFF) result &= (Ports::base[0] & Ports::wii[0]);
        if (~(portHigh | 0xFD)&0xFF) result &= (Ports::base[1] & Ports::wii[1]);
        if (~(portHigh | 0xFB)&0xFF) result &= (Ports::base[2] & Ports::wii[2]);
        if (~(portHigh | 0xF7)&0xFF) result &= (Ports::base[3] & Ports::wii[3]);
        if (~(portHigh | 0xEF)&0xFF) result &= (Ports::base[4] & Ports::wii[4]);
        if (~(portHigh | 0xDF)&0xFF) result &= (Ports::base[5] & Ports::wii[5]);
        if (~(portHigh | 0xBF)&0xFF) result &= (Ports::base[6] & Ports::wii[6]);
        if (~(portHigh | 0x7F)&0xFF) result &= (Ports::base[7] & Ports::wii[7]);

#ifdef ZX_KEYB_PRESENT
        detectZXKeyCombinationForMenu();
        
        uint8_t zxkbres = 0xFF;
        // output high part of address bus through pins for physical keyboard rows
        for (int b = 0; b < 8; b++) {
            digitalWrite(psKR[b], ((portHigh >> b) & 0x1) ? HIGH : LOW);
        }
        // delay for letting the value to build up
        // delay(2);
        // read keyboard rows
        bitWrite(zxkbres, 0, digitalRead(DB0));
        bitWrite(zxkbres, 1, digitalRead(DB1));
        bitWrite(zxkbres, 2, digitalRead(DB2));
        bitWrite(zxkbres, 3, digitalRead(DB3));
        bitWrite(zxkbres, 4, digitalRead(DB4));
        // combine physical keyboard value read
        result &= zxkbres;
#endif // ZX_KEYB_PRESENT

        return result;
    }
    // Kempston
    if (portLow == 0x1F) {
        return Ports::base[31];
    }
    // Sound (AY-3-8912)

#ifdef USE_AY_SOUND
    if (portLow == 0xFD) {
        switch (portHigh) {
        case 0xFF:
            // Serial.println("Read AY register");
            //return _ay3_8912.getRegisterData();
            return AySound::getRegisterData();
        }
    }
#endif

    uint8_t data = zx_data;
    data |= (0xe0); /* Set bits 5-7 - as reset above */
    data &= ~0x40;
    // Serial.printf("Port %x%x  Data %x\n", portHigh,portLow,data);
    return data;
}

extern "C" void output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    uint8_t tmp_data = data;
    switch (portLow) {
    case 0xFE: {

        // delayMicroseconds(CONTENTION_TIME);

        // border color (no bright colors)
        bitWrite(ESPectrum::borderColor, 0, bitRead(data, 0));
        bitWrite(ESPectrum::borderColor, 1, bitRead(data, 1));
        bitWrite(ESPectrum::borderColor, 2, bitRead(data, 2));

#ifdef SPEAKER_PRESENT
        digitalWrite(SPEAKER_PIN, bitRead(data, 4)); // speaker
#endif

#ifdef MIC_PRESENT
        digitalWrite(MIC_PIN, bitRead(data, 3)); // tape_out
#endif

        Ports::base[0x20] = data;
    } break;

    case 0xFD: {
        // Sound (AY-3-8912)
        switch (portHigh) {

#ifdef USE_AY_SOUND
        case 0xFF:
            // Serial.printf("Select AY register %x %x %x\n",portHigh,portLow,data);
            //_ay3_8912.selectRegister(data);
            AySound::selectRegister(data);
            break;
        case 0xBF:
            // Serial.printf("Select AY register Data %x %x %x\n",portHigh,portLow,data);
            //_ay3_8912.setRegisterData(data);
            AySound::setRegisterData(data);
            break;
#endif

        case 0x7F:
            if (!Mem::pagingLock) {
                Mem::pagingLock = bitRead(tmp_data, 5);
                Mem::romLatch = bitRead(tmp_data, 4);
                Mem::videoLatch = bitRead(tmp_data, 3);
                Mem::bankLatch = tmp_data & 0x7;
                // Mem::romInUse=0;
                bitWrite(Mem::romInUse, 1, Mem::romSP3);
                bitWrite(Mem::romInUse, 0, Mem::romLatch);
                // Serial.printf("7FFD data: %x ROM latch: %x Video Latch: %x bank latch: %x page lock:
                // %x\n",tmp_data,Mem::romLatch,Mem::videoLatch,Mem::bankLatch,Mem::pagingLock);
            }
            break;

        case 0x1F:
            Mem::modeSP3 = bitRead(data, 0);
            Mem::romSP3 = bitRead(data, 2);
            bitWrite(Mem::romInUse, 1, Mem::romSP3);
            bitWrite(Mem::romInUse, 0, Mem::romLatch);

            // Serial.printf("1FFD data: %x mode: %x rom bits: %x ROM chip: %x\n",data,Mem::modeSP3,Mem::romSP3, Mem::romInUse);
            break;
        }
    } break;
        // default:
        //    zx_data = data;
        break;
    }
}
