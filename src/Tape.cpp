#include "hardpins.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Tape.h"

String Tape::tapeFileName = "none";
byte Tape::tapeStatus = TAPE_STOPPED;

static uint8_t tapePhase;
static uint64_t tapeStart;
static uint32_t tapePulseCount;
static uint16_t tapeBitPulseLen;   
static uint8_t tapeBitPulseCount;     
static uint32_t tapebufByteCount;
static uint16_t tapeHdrPulses;
static uint16_t tapeBlockLen;
static size_t tapeFileSize;   
static uint8_t* tape;
static uint8_t tapeEarBit;
static uint8_t tapeBitMask;    

void Tape::Init()
{
    tape = (uint8_t*)ps_calloc(1, 0x100000); // 1 MB max TAP file size
    if (tape == NULL) Serial.printf("Error allocating tape into PSRAM\n");
}

boolean Tape::TAP_Load()
{
    
    File tapefile;

    tapefile = FileUtils::safeOpenFileRead(Tape::tapeFileName);
    tapeFileSize = readBlockFile(tapefile, tape, 0x100000);
    if (tapeFileSize == -1) return false;
    tapefile.close();

    return true;

}

uint8_t Tape::TAP_Play()
{
    
    if (Tape::tapeFileName== "none") return false;
    
    switch (Tape::tapeStatus) {
    case TAPE_STOPPED:
        tapePhase=TAPE_PHASE_SYNC;
        tapePulseCount=0;
        tapeEarBit=LOW;
        tapeBitMask=0x80;
        tapeBitPulseCount=0;
        tapeBitPulseLen=TAPE_BIT0_PULSELEN;
        tapeHdrPulses=TAPE_HDR_LONG;
        tapeBlockLen=(tape[0] | (tape[1] <<8)) + 2;
        tapebufByteCount=2;
        tapeStart=CPU::global_tstates;
        Tape::tapeStatus=TAPE_LOADING;
        break;
    case TAPE_LOADING:
        Tape::tapeStatus=TAPE_PAUSED;
        break;
    case TAPE_PAUSED:
        tapeStart=CPU::global_tstates;        
        Tape::tapeStatus=TAPE_LOADING;
    }
    
    return true;
}

uint8_t Tape::TAP_Read()
{
    uint64_t tapeCurrent = CPU::global_tstates - tapeStart;
    
    switch (tapePhase) {
    case TAPE_PHASE_SYNC:
        if (tapeCurrent > TAPE_SYNC_LEN) {
            tapeStart=CPU::global_tstates;
            tapeEarBit ^= 1;
            tapePulseCount++;
            if (tapePulseCount>tapeHdrPulses) {
                tapePulseCount=0;
                tapePhase=TAPE_PHASE_SYNC1;
            }
        }
        break;
    case TAPE_PHASE_SYNC1:
        if (tapeCurrent > TAPE_SYNC1_LEN) {
            tapeStart=CPU::global_tstates;
            tapeEarBit ^= 1;
            tapePhase=TAPE_PHASE_SYNC2;
        }
        break;
    case TAPE_PHASE_SYNC2:
        if (tapeCurrent > TAPE_SYNC2_LEN) {
            tapeStart=CPU::global_tstates;
            tapeEarBit ^= 1;
            if (tape[tapebufByteCount] & tapeBitMask) tapeBitPulseLen=TAPE_BIT1_PULSELEN; else tapeBitPulseLen=TAPE_BIT0_PULSELEN;            
            tapePhase=TAPE_PHASE_DATA;
        }
        break;
    case TAPE_PHASE_DATA:
        if (tapeCurrent > tapeBitPulseLen) {
            tapeStart=CPU::global_tstates;
            tapeEarBit ^= 1;
            tapeBitPulseCount++;
            if (tapeBitPulseCount==2) {
                tapeBitPulseCount=0;
                tapeBitMask = (tapeBitMask >>1 | tapeBitMask <<7);
                if (tapeBitMask==0x80) {
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        tapePhase=TAPE_PHASE_PAUSE;
                        tapeEarBit=LOW;
                        break;
                    }
                }
                if (tape[tapebufByteCount] & tapeBitMask) tapeBitPulseLen=TAPE_BIT1_PULSELEN; else tapeBitPulseLen=TAPE_BIT0_PULSELEN;
            }
        }
        break;
    case TAPE_PHASE_PAUSE:
        if (tapebufByteCount < tapeFileSize) {
            if (tapeCurrent > TAPE_BLK_PAUSELEN) {
                tapeStart=CPU::global_tstates;
                tapePulseCount=0;
                tapePhase=TAPE_PHASE_SYNC;
                tapeBlockLen+=(tape[tapebufByteCount] | tape[tapebufByteCount + 1] <<8)+ 2;
                tapebufByteCount+=2;
                if (tape[tapebufByteCount]) tapeHdrPulses=TAPE_HDR_SHORT; else tapeHdrPulses=TAPE_HDR_LONG;
            }
        } else Tape::tapeStatus=TAPE_STOPPED;
        return LOW;
    } 
    
#ifdef SPEAKER_PRESENT
    digitalWrite(SPEAKER_PIN, tapeEarBit); // Send tape load sound to speaker
#endif
    
    return tapeEarBit;
}