#include <Arduino.h>
#include <string.h>
#include <FS.h>
#include "FileUtils.h"
#include "hardpins.h"
#include "CPU.h"
#include "Tape.h"

// Tape
String Tape::tapeFileName = "none";
byte Tape::tapeStatus = TAPE_IDLE;
byte Tape::tapePhase = TAPE_PHASE_SYNC;
uint64_t Tape::tapeStart = 0;
byte Tape::tapeEarBit = 0;
uint32_t Tape::tapePulseCount = 0;
uint16_t Tape::tapeBitPulseLen = TAPE_BIT0_PULSELEN;
uint8_t Tape::tapeBitPulseCount=0;     
uint8_t Tape::tapeBitMask=0x80;    
uint32_t Tape::tapebufByteCount=0;
uint16_t Tape::tapeHdrPulses=TAPE_HDR_LONG; 
uint16_t Tape::tapeBlockLen=0;
size_t Tape::tapeFileSize=0;
uint8_t* Tape::tape = NULL;

void Tape::Init()
{
    Tape::tape = (uint8_t*)ps_calloc(1, 0x20000);
    if (Tape::tape == NULL) Serial.printf("Error allocating tape into PSRAM\n");
}

boolean Tape::TAP_Load()
{
    
    File tapefile;

    tapefile = FileUtils::safeOpenFileRead(Tape::tapeFileName);
    Tape::tapeFileSize = readBlockFile(tapefile, Tape::tape, 0x20000);
    if (Tape::tapeFileSize == -1) return false;

    tapefile.close();

    return true;

}

uint8_t Tape::TAP_Play()
{
    
    if (Tape::tapeFileName== "none") return false;
    
    Tape::tapePhase=TAPE_PHASE_SYNC;
    Tape::tapeEarBit=1;
    Tape::tapePulseCount=0;
    Tape::tapeBitPulseCount=0;
    Tape::tapeBitMask=0x80;    
    Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;
    Tape::tapeHdrPulses=TAPE_HDR_LONG;
    Tape::tapeBlockLen=(Tape::tape[0] | (Tape::tape[1] <<8))+ 2;
    Tape::tapebufByteCount=2;
    Tape::tapeStart=CPU::global_tstates;
    Tape::tapeStatus=TAPE_LOADING;

    return true;
}

uint8_t Tape::TAP_Read()
{
    uint64_t tapeCurrent = CPU::global_tstates - Tape::tapeStart;
    
    switch (Tape::tapePhase) {
    case TAPE_PHASE_SYNC:
        if (tapeCurrent > TAPE_SYNC_LEN) {
            Tape::tapeStart=CPU::global_tstates;
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapePulseCount++;
            if (Tape::tapePulseCount>Tape::tapeHdrPulses) {
                Tape::tapePulseCount=0;
                Tape::tapePhase=TAPE_PHASE_SYNC1;
            }
        }
        break;
    case TAPE_PHASE_SYNC1:
        if (tapeCurrent > TAPE_SYNC1_LEN) {
            Tape::tapeStart=CPU::global_tstates;
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapePhase=TAPE_PHASE_SYNC2;
        }
        break;
    case TAPE_PHASE_SYNC2:
        if (tapeCurrent > TAPE_SYNC2_LEN) {
            Tape::tapeStart=CPU::global_tstates;
            Tape:: tapeEarBit ^= 1UL;
            if (Tape::tape[tapebufByteCount] & Tape::tapeBitMask) Tape::tapeBitPulseLen=TAPE_BIT1_PULSELEN; else Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;            
            Tape::tapePhase=TAPE_PHASE_DATA;
        }
        break;
    case TAPE_PHASE_DATA:
        if (tapeCurrent > Tape::tapeBitPulseLen) {
            Tape::tapeStart=CPU::global_tstates;
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapeBitPulseCount++;
            if (Tape::tapeBitPulseCount==2) {
                Tape::tapeBitPulseCount=0;
                Tape::tapeBitMask = Tape::tapeBitMask >> 1UL;
                if (Tape::tapeBitMask==0) {
                    Tape::tapeBitMask=0x80;
                    Tape::tapebufByteCount++;
                    if (Tape::tapebufByteCount == Tape::tapeBlockLen) {
                        Tape::tapePhase=TAPE_PHASE_PAUSE;
                        break;
                    }
                }
                if (Tape::tape[Tape::tapebufByteCount] & Tape::tapeBitMask) Tape::tapeBitPulseLen=TAPE_BIT1_PULSELEN; else Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;
            }
        }
        break;
    case TAPE_PHASE_PAUSE:
        if (Tape::tapebufByteCount < Tape::tapeFileSize) {
            if (tapeCurrent > TAPE_BLK_PAUSELEN) {
                Tape::tapeStart=CPU::global_tstates;
                Tape::tapeEarBit=1;
                Tape::tapePulseCount=0;
                Tape::tapePhase=TAPE_PHASE_SYNC;

                Tape::tapeBlockLen+=(Tape::tape[Tape::tapebufByteCount] | Tape::tape[Tape::tapebufByteCount + 1] <<8)+ 2;
                Tape::tapebufByteCount+=2;
                if (Tape::tape[Tape::tapebufByteCount]) Tape::tapeHdrPulses=TAPE_HDR_SHORT; else Tape::tapeHdrPulses=TAPE_HDR_LONG;

            } else return 1;              
        } else {
            Tape::tapeStatus=TAPE_IDLE;
            return 0;
        }
    } 
    
#ifdef SPEAKER_PRESENT
    digitalWrite(SPEAKER_PIN, Tape::tapeEarBit); // Send tape load sound to speaker
#endif
    
    return Tape::tapeEarBit;
}