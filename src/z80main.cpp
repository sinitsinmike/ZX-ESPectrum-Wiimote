#include "hardpins.h"
#include <stdio.h>
#include <string.h>

#include "ESPectrum.h"

#include "PS2Kbd.h"
#include "z80main.h"
#include "Config.h"
#include "AySound.h"

#pragma GCC optimize ("O3")

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
static uint8_t port_data = 0;

Z80_STATE _zxCpu;

#ifdef CPU_LINKEFONG

CONTEXT _zxContext;

extern "C" {
    uint8_t readbyte(uint16_t addr);
    uint16_t readword(uint16_t addr);
    void writebyte(uint16_t addr, uint8_t data);
    void writeword(uint16_t addr, uint16_t data);
    uint8_t input(uint8_t portLow, uint8_t portHigh);
    void output(uint8_t portLow, uint8_t portHigh, uint8_t data);
}

#endif  // CPU_LINKEFONG

#ifdef CPU_JLSANCHEZ

extern "C" {
    uint8_t input(uint8_t portLow, uint8_t portHigh);
    void output(uint8_t portLow, uint8_t portHigh, uint8_t data);
}

#include "z80.h"

static bool interruptPending = false;
static uint32_t tstates = 0;

// sequence of wait states
static unsigned char wait_states[8] = { 6, 5, 4, 3, 2, 1, 0, 0 };

// delay contention: emulates wait states introduced by the ULA (graphic chip)
// whenever there is a memory access to contended memory (shared between ULA and CPU).
// detailed info: https://worldofspectrum.org/faq/reference/48kreference.htm#ZXSpectrum
// from paragraph which starts with "The 50 Hz interrupt is synchronized with..."
// if you only read from https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
// without reading the previous paragraphs about line timings, it may be confusing.
static unsigned char delay_contention(uint16_t address, unsigned int tstates)
{
	// delay states one t-state BEFORE the first pixel to be drawn
	tstates += 1;

	// each line spans 224 t-states
	int line = tstates / 224;

	// only the 192 lines between 64 and 255 have graphic data, the rest is border
	if (line < 64 || line >= 256) return 0;

	// only the first 128 t-states of each line correspond to a graphic data transfer
	// the remaining 96 t-states correspond to border
	int halfpix = tstates % 224;
	if (halfpix >= 128) return 0;

	int modulo = halfpix % 8;
	return wait_states[modulo];
}

#define ADDRESS_IN_LOW_RAM(addr) (1 == (addr >> 14))

class MyZ80 : public Z80operations
{
    public:
    /* Read opcode from RAM */
    uint8_t fetchOpcode(uint16_t address) {
        // 3 clocks to fetch opcode from RAM and 1 execution clock
        if (ADDRESS_IN_LOW_RAM(address))
            tstates += delay_contention(address, tstates);

        tstates += 4;
        return readbyte(address);
    }

    /* Read/Write byte from/to RAM */
    uint8_t peek8(uint16_t address) {
        // 3 clocks for read byte from RAM
        if (ADDRESS_IN_LOW_RAM(address))
            tstates += delay_contention(address, tstates);

        tstates += 3;
        return readbyte(address);
    }
    void poke8(uint16_t address, uint8_t value) {
        // 3 clocks for write byte to RAM
        if (ADDRESS_IN_LOW_RAM(address))
            tstates += delay_contention(address, tstates);

        tstates += 3;
        writebyte(address, value);
    }

    /* Read/Write word from/to RAM */
    uint16_t peek16(uint16_t address) {
        // Order matters, first read lsb, then read msb, don't "optimize"
        uint8_t lsb = peek8(address);
        uint8_t msb = peek8(address + 1);
        return (msb << 8) | lsb;
    }
    void poke16(uint16_t address, RegisterPair word) {
        // Order matters, first write lsb, then write msb, don't "optimize"
        poke8(address, word.byte8.lo);
        poke8(address + 1, word.byte8.hi);
    }

    /* In/Out byte from/to IO Bus */
    uint8_t inPort(uint16_t port) {
        // 3 clocks for read byte from bus
        tstates += 3;
        uint8_t hiport = port >> 8;
        uint8_t loport = port & 0xFF;
        return input(loport, hiport);
    }
    void outPort(uint16_t port, uint8_t value) {
        // 4 clocks for write byte to bus
        tstates += 4;
        uint8_t hiport = port >> 8;
        uint8_t loport = port & 0xFF;
        output(loport, hiport, value);
    }

    /* Put an address on bus lasting 'tstates' cycles */
    void addressOnBus(uint16_t address, int32_t wstates){
    	// Additional clocks to be added on some instructions
        if (ADDRESS_IN_LOW_RAM(address)) {
            for (int idx = 0; idx < wstates; idx++) {
                tstates += delay_contention(address, tstates) + 1;
            }
        }
        else
            tstates += wstates;
    }

    /* Clocks needed for processing INT and NMI */
    void interruptHandlingTime(int32_t wstates) {
    	tstates += wstates;
    }

    /* Callback to know when the INT signal is active */
    bool isActiveINT(void) {
        if (!interruptPending) return false;
        interruptPending = false;
        return true;
    }
};

MyZ80 myZ80;
Z80 z80(&myZ80);

#endif  // CPU_JLSANCHEZ

void zx_setup() {
#ifdef CPU_LINKEFONG
    _zxContext.readbyte = readbyte;
    _zxContext.readword = readword;
    _zxContext.writeword = writeword;
    _zxContext.writebyte = writebyte;
    _zxContext.input = input;
    _zxContext.output = output;
#endif

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

#ifdef CPU_LINKEFONG
    Z80Reset(&_zxCpu);
#endif

#ifdef CPU_JLSANCHEZ
    z80.reset();
#endif 
}

#ifdef CPU_JLSANCHEZ

#define USE_PER_INSTRUCTION_TIMING

//#define LOG_DEBUG_TIMING

#ifdef USE_PER_INSTRUCTION_TIMING

static uint32_t ts_start;
static uint32_t ts_target_frame = 20000;
static uint32_t target_cycle_count;

static inline void begin_timing(uint32_t _target_cycle_count)
{
    ts_start = micros();
    target_cycle_count = _target_cycle_count;

#ifdef LOG_DEBUG_TIMING
    static int ctr = 0;
    if (ctr == 0) {
        ctr = 50;
        Serial.printf("begin_timing: %u\n", _target_cycle_count);
    }
    else ctr--;
#endif
}

static inline void delay_instruction(uint32_t elapsed_cycles)
{
    uint32_t ts_current = micros() - ts_start;
    uint32_t ts_target = ts_target_frame * elapsed_cycles / target_cycle_count;
    if (ts_target > ts_current) {
        uint32_t us_to_wait = ts_target - ts_current;
        if (us_to_wait < ts_target_frame)
            delayMicroseconds(us_to_wait);
    }

#ifdef LOG_DEBUG_TIMING
    static int ctr = 0;
    if (ctr == 0) {
        ctr = 200000;
        Serial.printf("ts_current: Tstate = %u, ts_target = %u, ts_current = %u\n", elapsed_cycles, ts_target, ts_current);
    }
    else ctr--;
#endif
}

#endif  // USE_PER_INSTRUCTION_TIMING

#endif  // CPU_JLSANCHEZ

int32_t zx_loop()
{
    int32_t result = -1;

#ifdef CPU_LINKEFONG
    Z80Emulate(&_zxCpu, cycles_per_step, &_zxContext);
    Z80Interrupt(&_zxCpu, 0xff, &_zxContext);
#endif
#ifdef CPU_JLSANCHEZ
    uint32_t cycleTstates = cycles_per_step;

#ifdef USE_PER_INSTRUCTION_TIMING
    begin_timing(cycleTstates);
#endif

    tstates = 0;
	uint32_t prevTstates = 0;

	while (tstates < cycleTstates)
	{
		z80.execute();
#ifdef USE_PER_INSTRUCTION_TIMING
        delay_instruction(tstates);
#endif
		prevTstates = tstates;
	}

    interruptPending = true;
#endif
    return result;
}

extern "C" uint8_t readbyte(uint16_t addr) {
    uint8_t page = addr >> 14;
    switch (page) {
    case 0:
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
    case 1:
        return Mem::ram5[addr - 0x4000];
        break;
    case 2:
        return Mem::ram2[addr - 0x8000];
        break;
    case 3:
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
    uint8_t page = addr >> 14;
    switch (page) {
    case 0:
        return;
    case 1:
        Mem::ram5[addr - 0x4000] = data;
        break;
    case 2:
        Mem::ram2[addr - 0x8000] = data;
        break;
    case 3:
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

    uint8_t data = port_data;
    data |= (0xe0); /* Set bits 5-7 - as reset above */
    data &= ~0x40;
    // Serial.printf("Port %x%x  Data %x\n", portHigh,portLow,data);
    return data;
}

extern "C" void output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    uint8_t tmp_data = data;
    switch (portLow) {
    case 0xFE:
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
        break;

    case 0xFD: {
        // Sound (AY-3-8912)
        switch (portHigh) {
#ifdef USE_AY_SOUND
            case 0xFF:
                AySound::selectRegister(data);
                break;
            case 0xBF:
                AySound::setRegisterData(data);
                break;
#endif
            case 0x7F:
                if (!Mem::pagingLock) {
                    Mem::pagingLock = bitRead(tmp_data, 5);
                    Mem::romLatch = bitRead(tmp_data, 4);
                    Mem::videoLatch = bitRead(tmp_data, 3);
                    Mem::bankLatch = tmp_data & 0x7;
                    bitWrite(Mem::romInUse, 1, Mem::romSP3);
                    bitWrite(Mem::romInUse, 0, Mem::romLatch);
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
        }
        break;
        // default:
        //    port_data = data;
        break;
    }
}
