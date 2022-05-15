#include <Arduino.h>
#include <string.h>
#include <FS.h>
#include "FileUtils.h"
#include "Tape.h"

// Tape
String Tape::tapeFileName = "none";
bool Tape::tapeAlive=true;
byte Tape::tapeStatus = 0;
byte Tape::tapePhase = 1;
unsigned long Tape::tapeSyncLen = 620;
unsigned long Tape::tapeStart = 0;
byte Tape::tapeEarBit = 0;
uint32_t Tape::tapePulseCount = 0;
uint16_t Tape::tapeBitPulseLen = 244;
uint8_t Tape::tapeBitPulseCount=0;     
uint8_t Tape::tapebufBitCount=0;         
uint32_t Tape::tapebufByteCount=0;
uint16_t Tape::tapeHdrPulses=8063; // 8063 for header, 3223 for data
uint16_t Tape::tapeBlockLen=0;
size_t Tape::tapeFileSize=0;
File Tape::tapefile;
uint8_t Tape::tapeCurrentByte=0; 

uint8_t Tape::TAP_Play()
{

        if (Tape::tapeFileName== "none") return false;

        //Tape::tapefile = FileUtils::safeOpenFileRead("/tap/teclado.tap"); 
        Tape::tapefile = FileUtils::safeOpenFileRead(Tape::tapeFileName);
        Tape::tapeFileSize = Tape::tapefile.size();
        Tape::tapePhase=1;
        Tape::tapeEarBit=1;
        Tape::tapePulseCount=0;
        Tape::tapeBitPulseLen=244;
        Tape::tapeBitPulseCount=0;
        Tape::tapebufBitCount=0;         
        Tape::tapebufByteCount=3;
        Tape::tapeHdrPulses=8063;
        Tape::tapeSyncLen=619;
        Tape::tapeBlockLen=(readByteFile(Tape::tapefile) | (readByteFile(Tape::tapefile) <<8))+ 2;
        Tape::tapeCurrentByte=readByteFile(Tape::tapefile); 
        Tape::tapeStart=micros();
        Tape::tapeStatus=TAPE_LOADING; // START LOAD
 
        return true;
}
