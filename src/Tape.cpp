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
