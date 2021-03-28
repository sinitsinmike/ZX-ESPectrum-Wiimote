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

uint8_t Ports::input(uint8_t portLow, uint8_t portHigh)
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

        return result;
    }
    // Kempston
    if (portLow == 0x1F) {
        return base[31];
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

void Ports::output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    // Serial.printf("%02X,%02X:%02X|", portHigh, portLow, data);
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
        base[0x20] = data;
        break;

    case 0xFD: {
        // Sound (AY-3-8912)
        switch (portHigh) {
#ifdef USE_AY_SOUND
            case 0xFF:
                AySound::selectRegister(data);
                break;
            case 0xBF:
            case 0xBE:
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
