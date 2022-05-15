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
#include "Ports.h"
#include "Mem.h"
#include "PS2Kbd.h"
#include "AySound.h"
#include "ESPectrum.h"
#include "CPU.h"
#include "FileUtils.h"
#include "Tape.h"

#include <Arduino.h>

// Ports
volatile uint8_t Ports::base[128];
volatile uint8_t Ports::wii[128];

static uint8_t port_data = 0;

#ifdef ZX_KEYB_PRESENT
const int psKR[] = {AD8, AD9, AD10, AD11, AD12, AD13, AD14, AD15};
const int psKC[] = {DB0, DB1, DB2, DB3, DB4};
static void detectZXKeyCombinationForMenu()
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

// using partial port address decoding
// see https://worldofspectrum.org/faq/reference/ports.htm
//
//        Peripheral: 48K ULA
//        Port: ---- ---- ---- ---0
//
//        Peripheral: Kempston Joystick.
//        Port: ---- ---- 000- ---- 
//
//        Peripheral: 128K AY Register
//        Port: 11-- ---- ---- --0-
//
//        Peripheral: 128K AY (Data)
//        Port: 10-- ---- ---- --0-
//
//        Peripheral: ZX Spectrum 128K / +2 Memory Control
//        Port: 0--- ---- ---- --0-
//
//        Peripheral: ZX Spectrum +2A / +3 Primary Memory Control
//        Port: 01-- ---- ---- --0-
//
//        Peripheral: ZX Spectrum +2A / +3 Secondary Memory Control
//        Port: 0001 ---- ---- --0-
//


uint8_t Ports::input(uint8_t portLow, uint8_t portHigh)
{
    // 48K ULA
    if ((portLow & 0x01) == 0x00) // (portLow == 0xFE) 
    {
        // all result bits initially set to 1, may be set to 0 eventually
        uint8_t result = 0xFF;

        #ifdef EAR_PRESENT
        // EAR_PIN
        if (portHigh == 0xFE) {
            bitWrite(result, 6, digitalRead(EAR_PIN));
        }
        #endif

        // Keyboard
        if (~(portHigh | 0xFE)&0xFF) result &= (base[0] & wii[0]);
        if (~(portHigh | 0xFD)&0xFF) result &= (base[1] & wii[1]);
        if (~(portHigh | 0xFB)&0xFF) result &= (base[2] & wii[2]);
        if (~(portHigh | 0xF7)&0xFF) result &= (base[3] & wii[3]);
        if (~(portHigh | 0xEF)&0xFF) result &= (base[4] & wii[4]);
        if (~(portHigh | 0xDF)&0xFF) result &= (base[5] & wii[5]);
        if (~(portHigh | 0xBF)&0xFF) result &= (base[6] & wii[6]);
        if (~(portHigh | 0x7F)&0xFF) result &= (base[7] & wii[7]);

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

        if (Tape::tapeStatus==TAPE_LOADING) {
            
            unsigned long tapeCurrent = micros() - Tape::tapeStart;

            switch (Tape::tapePhase) {
            case 1:
                // 619 -> microseconds for 2168 tStates (48K)
                if (tapeCurrent > Tape::tapeSyncLen) {
                    Tape::tapeStart=micros();
                    if (Tape::tapeEarBit) Tape::tapeEarBit=0; else Tape::tapeEarBit=1;
                    Tape::tapePulseCount++;
                    if (Tape::tapePulseCount>Tape::tapeHdrPulses) {
                        Tape::tapePulseCount=0;
                        Tape::tapePhase++;
                    }
                }
                break;
            case 2: // SYNC 1
                // 190 -> microseconds for 667 tStates (48K)
                if (tapeCurrent > 190) {
                    Tape::tapeStart=micros();
                    if (Tape::tapeEarBit) Tape::tapeEarBit=0; else Tape::tapeEarBit=1;
                    Tape::tapePulseCount++;
                    if (Tape::tapePulseCount==1) {
                        Tape::tapePulseCount=0;
                        Tape::tapePhase++;
                    }
                }
                break;
            case 3: // SYNC 2
                // 210 -> microseconds for 735 tStates (48K)
                if (tapeCurrent > 210) {
                    Tape::tapeStart=micros();
                    if (Tape::tapeEarBit) Tape::tapeEarBit=0; else Tape::tapeEarBit=1;
                    Tape::tapePulseCount++;
                    if (Tape::tapePulseCount==1) {
                        Tape::tapePulseCount=0;
                        Tape::tapePhase++;

                        /*
                        int i=0;
                        for (i=0; i<32; i++) {
                         Serial.printf("%02X ",Mem::tapeBuffer[Tape::tapebufByteCount+i]);
                        }
                        Serial.printf("\n");
                        */

                        // Leer primer bit de datos de cabecera para indicar a la fase 4 longitud de pulso
                        if (Tape::tapeCurrentByte >> 7) Tape::tapeBitPulseLen=488; else Tape::tapeBitPulseLen=244;                        
                        
                        //if (Mem::tapeBuffer[Tape::tapebufByteCount] >> 7) Tape::tapeBitPulseLen=488; else Tape::tapeBitPulseLen=244;

                    }
                }
                break;
            case 4:

                if (tapeCurrent > Tape::tapeBitPulseLen) {

                    Tape::tapeStart=micros();
                    if (Tape::tapeEarBit) Tape::tapeEarBit=0; else Tape::tapeEarBit=1;

                    Tape::tapeBitPulseCount++;
                    if (Tape::tapeBitPulseCount==2) {
                        Tape::tapebufBitCount++;
                        if (Tape::tapebufBitCount==8) {
                            Tape::tapebufBitCount=0;
                            Tape::tapebufByteCount++;
                            Tape::tapeCurrentByte=readByteFile(Tape::tapefile);
                        }
                        
                        if ((Tape::tapeCurrentByte >> (7 - Tape::tapebufBitCount)) & 0x01) Tape::tapeBitPulseLen=488; else Tape::tapeBitPulseLen=244;                        
                        
                        //if ((Mem::tapeBuffer[Tape::tapebufByteCount] >> (7 - Tape::tapebufBitCount)) & 0x01) Tape::tapeBitPulseLen=488; else Tape::tapeBitPulseLen=244;                        
                        
                        Tape::tapeBitPulseCount=0;
                    }
                    
                    if (Tape::tapebufByteCount > Tape::tapeBlockLen) { // FIN DE BLOQUE, SALIMOS A PAUSA ENTRE BLOQUES
                        Tape::tapePhase++;
                        Tape::tapeStart=micros();

                        Serial.printf("%02X\n",Tape::tapeCurrentByte);

                    }

                }
                break;
            case 5:
                if (Tape::tapebufByteCount < Tape::tapeFileSize) {
                    if (tapeCurrent > 500000UL) {                        
                        Tape::tapeStart=micros();
                        Tape::tapeEarBit=1;
                        Tape::tapeBitPulseCount=0;
                        Tape::tapePulseCount=0;
                        Tape::tapePhase=1;
                        
                        Tape::tapeBlockLen+=(Tape::tapeCurrentByte | (readByteFile(Tape::tapefile) <<8))+ 2;
                        //Tape::tapeBlockLen+=(Mem::tapeBuffer[Tape::tapebufByteCount] | (Mem::tapeBuffer[(Tape::tapebufByteCount)+1] <<8)) + 2;

//                        int i=0;
//                        for (i=0; i<32; i++) {
                            Serial.printf("%02X ",Tape::tapeCurrentByte);
//                        }
                        Serial.printf("\n");

                        Tape::tapebufByteCount+=2;
                        Tape::tapebufBitCount=0;

                        Tape::tapeCurrentByte=readByteFile(Tape::tapefile);
                        //Tape::tapebufByteCount++; // Not sure                        
                        if (Tape::tapeCurrentByte) {
                            Tape::tapeHdrPulses=3223; 
                        } else {
                            Tape::tapeHdrPulses=8063;
                        }
/*                        
                        if (Mem::tapeBuffer[Tape::tapebufByteCount]) {
                            Tape::tapeHdrPulses=3223; 
                        } else {
                            Tape::tapeHdrPulses=8063;
                        }
  */                  
                    } else {
                        result &= 0xbf; result |= 0xe0;
                        return result;
                    }
                } else {
                    
                    Serial.printf("%u\n",Tape::tapebufByteCount);
                    Serial.printf("%u\n",Tape::tapeBlockLen);                    
                    Serial.printf("%u\n",Tape::tapeFileSize);
                    
                    Tape::tapeStatus=TAPE_IDLE;
                    Tape::tapefile.close();
                    result &= 0xbf;        
                    if (base[0x20] & 0x18) result |= (0xe0); else result |= (0xa0); // ISSUE 2 behaviour
                    return result;
                }
                break;
            } 
            result |= 0xa0;
            bitWrite(result,6,(Tape::tapeEarBit << 6));
            digitalWrite(SPEAKER_PIN, bitRead(result,6)); // Send tape load sound to speaker
            return result;
        }
        
        result &= 0xbf;        
        if (base[0x20] & 0x18) result |= (0xe0); else result |= (0xa0); // ISSUE 2 behaviour
        return result;
    }

    // Kempston
    if ((portLow & 0xE0) == 0x00) // (portLow == 0x1F)
    {
        return base[31];
    }

    // Sound (AY-3-8912)
    #ifdef USE_AY_SOUND
    if ((portHigh & 0xC0) == 0xC0 && (portLow & 0x02) == 0x00)  // 0xFFFD
    {
        return AySound::getRegisterData();
    }
    #endif

    uint8_t data = port_data;
    data |= (0xe0); /* Set bits 5-7 - as reset above */
    data &= ~0x40;
    // Serial.printf("Port %x%x  Data %x\n", portHigh,portLow,data);
    return data;
}

void Ports::output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    // Serial.printf("%02X,%02X:%02X|", portHigh, portLow, data);

    // 48K ULA
    if ((portLow & 0x01) == 0x00)
    {
        ESPectrum::borderColor = data & 0x07;

        #ifdef SPEAKER_PRESENT
        if (Tape::tapeStatus==TAPE_SAVING) {
            digitalWrite(SPEAKER_PIN, bitRead(data, 3)); // re-route tape out data to speaker
        } else {
            digitalWrite(SPEAKER_PIN, bitRead(data, 4)); // speaker
        }
        #endif

        #ifdef MIC_PRESENT
        digitalWrite(MIC_PIN, bitRead(data, 3)); // tape_out
        #endif
        if(Tape::tapeStatus==TAPE_LOADING) base[0x20] = 0; else base[0x20] = data; // ? 
    }
    
    if ((portLow & 0x02) == 0x00)
    {
        // 128K AY
        #ifdef USE_AY_SOUND
        if ((portHigh & 0x80) == 0x80)
        {
            if ((portHigh & 0x40) == 0x40)
                AySound::selectRegister(data);
            else
                AySound::setRegisterData(data);
        }
        #endif

        // will decode both
        // 128K / +2 Memory Control
        // +2A / +3 Memory Control
        if ((portHigh & 0xC0) == 0x40)
        {
            if (!Mem::pagingLock) {
                Mem::pagingLock = bitRead(data, 5);
                Mem::romLatch = bitRead(data, 4);
                Mem::videoLatch = bitRead(data, 3);
                Mem::bankLatch = data & 0x7;
                bitWrite(Mem::romInUse, 1, Mem::romSP3);
                bitWrite(Mem::romInUse, 0, Mem::romLatch);
            }
        }
        
        // +2A / +3 Secondary Memory Control
        if ((portHigh & 0xF0) == 0x01)
        {
            Mem::modeSP3 = bitRead(data, 0);
            Mem::romSP3 = bitRead(data, 2);
            bitWrite(Mem::romInUse, 1, Mem::romSP3);
            bitWrite(Mem::romInUse, 0, Mem::romLatch);
        }

    }
    
}
