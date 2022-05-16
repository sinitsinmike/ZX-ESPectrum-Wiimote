#include <Arduino.h>
#include <string.h>
#include <FS.h>
#include "FileUtils.h"
#include "hardpins.h"
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

uint8_t Tape::TAP_Read()
{

            unsigned long tapeCurrent = micros() - Tape::tapeStart;

            uint8_t tape_result=0x00;

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

                        // Leer primer bit de datos de cabecera para indicar a la fase 4 longitud de pulso
                        if (Tape::tapeCurrentByte >> 7) Tape::tapeBitPulseLen=488; else Tape::tapeBitPulseLen=244;                        

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
                        
                        Tape::tapeBitPulseCount=0;
                    }
                    
                    if (Tape::tapebufByteCount > Tape::tapeBlockLen) { // FIN DE BLOQUE, SALIMOS A PAUSA ENTRE BLOQUES
                        Tape::tapePhase++;
                        Tape::tapeStart=micros();

                        //Serial.printf("%02X\n",Tape::tapeCurrentByte);

                    }

                }
                break;
            case 5:
                if (Tape::tapebufByteCount < Tape::tapeFileSize) {
                    
                    if (tapeCurrent > 500000UL) { // 1/2 sec. of pause between blocks                       
                        Tape::tapeStart=micros();
                        Tape::tapeEarBit=1;
                        Tape::tapeBitPulseCount=0;
                        Tape::tapePulseCount=0;
                        Tape::tapePhase=1;
                        
                        Tape::tapeBlockLen+=(Tape::tapeCurrentByte | (readByteFile(Tape::tapefile) <<8))+ 2;

                        //Serial.printf("%02X ",Tape::tapeCurrentByte);
                        //Serial.printf("\n");

                        Tape::tapebufByteCount+=2;
                        Tape::tapebufBitCount=0;

                        Tape::tapeCurrentByte=readByteFile(Tape::tapefile);

                        if (Tape::tapeCurrentByte) {
                            Tape::tapeHdrPulses=3223; 
                        } else {
                            Tape::tapeHdrPulses=8063;
                        }

                    } else return tape_result;

                } else {
                    
                    //Serial.printf("%u\n",Tape::tapebufByteCount);
                    //Serial.printf("%u\n",Tape::tapeBlockLen);                    
                    //Serial.printf("%u\n",Tape::tapeFileSize);
                    
                    Tape::tapeStatus=TAPE_IDLE;
                    Tape::tapefile.close();

                    return tape_result;

                }
                break;
            } 

            bitWrite(tape_result,6,(Tape::tapeEarBit << 6));
            digitalWrite(SPEAKER_PIN, bitRead(tape_result,6)); // Send tape load sound to speaker
            
            return tape_result;

}