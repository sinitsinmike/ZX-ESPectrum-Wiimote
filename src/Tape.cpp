#include <Arduino.h>
#include <string.h>
#include <FS.h>
#include "FileUtils.h"
#include "hardpins.h"
#include "Tape.h"

// Tape
String Tape::tapeFileName = "none";
byte Tape::tapeStatus = 0;
byte Tape::tapePhase = 1;
unsigned long Tape::tapeStart = 0;
byte Tape::tapeEarBit = 0;
uint32_t Tape::tapePulseCount = 0;
uint16_t Tape::tapeBitPulseLen = TAPE_BIT0_PULSELEN;
uint8_t Tape::tapeBitPulseCount=0;     
uint8_t Tape::tapebufBitCount=0;         
uint32_t Tape::tapebufByteCount=0;
uint16_t Tape::tapeHdrPulses=TAPE_HDR_LONG; 
uint16_t Tape::tapeBlockLen=0;
size_t Tape::tapeFileSize=0;
File Tape::tapefile;
uint8_t Tape::tapeCurrentByte=0; 

uint8_t Tape::TAP_Play()
{
    if (Tape::tapeFileName== "none") return false;

    Tape::tapefile = FileUtils::safeOpenFileRead(Tape::tapeFileName);
    Tape::tapeFileSize = Tape::tapefile.size();
    Tape::tapePhase=1;
    Tape::tapeEarBit=1;
    Tape::tapePulseCount=0;
    Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;
    Tape::tapeBitPulseCount=0;
    Tape::tapeHdrPulses=TAPE_HDR_LONG;
    Tape::tapeBlockLen=(readByteFile(Tape::tapefile) | (readByteFile(Tape::tapefile) <<8))+ 2;
    Tape::tapeCurrentByte=readByteFile(Tape::tapefile); 
    Tape::tapebufByteCount=3;        
    Tape::tapebufBitCount=0;         

    Tape::tapeStart=micros();
    Tape::tapeStatus=TAPE_LOADING; // START LOAD

    return true;
}

uint8_t Tape::TAP_Read()
{
        uint8_t tape_result=0x00;
    unsigned long tapeCurrent = micros() - Tape::tapeStart;
    
    switch (Tape::tapePhase) {
    case 1: // SYNC
        if (tapeCurrent > TAPE_SYNC_LEN) {
            Tape::tapeStart=micros();
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapePulseCount++;
            if (Tape::tapePulseCount>Tape::tapeHdrPulses) {
                Tape::tapePulseCount=0;
                Tape::tapePhase++;
            }
        }
        break;
    case 2: // SYNC 1
        if (tapeCurrent > TAPE_SYNC1_LEN) {
            Tape::tapeStart=micros();
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapePhase++;
        }
        break;
    case 3: // SYNC 2
        if (tapeCurrent > TAPE_SYNC2_LEN) {
            Tape::tapeStart=micros();
            Tape:: tapeEarBit ^= 1UL;
            // Read 1st data bit of header for selecting pulse len
            if (Tape::tapeCurrentByte >> 7) Tape::tapeBitPulseLen=TAPE_BIT1_PULSELEN; else Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;                        
            Tape::tapePhase++;
        }
        break;
    case 4: // DATA
        if (tapeCurrent > Tape::tapeBitPulseLen) {
            Tape::tapeStart=micros();
            Tape:: tapeEarBit ^= 1UL;
            Tape::tapeBitPulseCount++;
            if (Tape::tapeBitPulseCount==2) {
                Tape::tapebufBitCount++;
                if (Tape::tapebufBitCount==8) {
                    Tape::tapebufBitCount=0;
                    Tape::tapebufByteCount++;
                    Tape::tapeCurrentByte=readByteFile(Tape::tapefile);
                }
                if ((Tape::tapeCurrentByte >> (7 - Tape::tapebufBitCount)) & 0x01) Tape::tapeBitPulseLen=TAPE_BIT1_PULSELEN; else Tape::tapeBitPulseLen=TAPE_BIT0_PULSELEN;                        
                Tape::tapeBitPulseCount=0;
            }
            if (Tape::tapebufByteCount > Tape::tapeBlockLen) {
                Tape::tapePhase++;
                Tape::tapeStart=micros();
            }
        }
        break;
    case 5: // PAUSE
        if (Tape::tapebufByteCount < Tape::tapeFileSize) {
            if (tapeCurrent > TAPE_BLK_PAUSELEN) {
                Tape::tapeStart=micros();
                Tape::tapeEarBit=1;
                Tape::tapeBitPulseCount=0;
                Tape::tapePulseCount=0;
                Tape::tapePhase=1;
                Tape::tapeBlockLen+=(Tape::tapeCurrentByte | (readByteFile(Tape::tapefile) <<8))+ 2;
                Tape::tapeCurrentByte=readByteFile(Tape::tapefile);
                Tape::tapebufByteCount+=2;
                Tape::tapebufBitCount=0;
                if (Tape::tapeCurrentByte) {
                    Tape::tapeHdrPulses=TAPE_HDR_SHORT; 
                } else {
                    Tape::tapeHdrPulses=TAPE_HDR_LONG;
                }
            } else return tape_result;
        } else {
            Tape::tapeStatus=TAPE_IDLE;
            Tape::tapefile.close();
            return tape_result;
        }
        break;
    } 
    bitWrite(tape_result,6,Tape::tapeEarBit);
    digitalWrite(SPEAKER_PIN, Tape::tapeEarBit); // Send tape load sound to speaker
    return tape_result;
}