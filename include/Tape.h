#include <FS.h>

#ifndef Tape_h
#define Tape_h

// Tape status definitions
#define TAPE_IDLE 0
#define TAPE_LOADING 1
#define TAPE_SAVING 2

class Tape
{
public:

    // Tape
    static String tapeFileName;
    static bool tapeAlive;
    static uint8_t tapeStatus;
    static uint8_t tapePhase;
    static unsigned long tapeSyncLen;
    static unsigned long tapeStart;
    static uint8_t tapeEarBit;
    static uint32_t tapePulseCount;
    static uint16_t tapeBitPulseLen;   
    static uint8_t tapeBitPulseCount;     
    static uint8_t tapebufBitCount;         
    static uint32_t tapebufByteCount;
    static uint16_t tapeHdrPulses;
    static uint16_t tapeBlockLen;
    static size_t tapeFileSize;   
    static File tapefile;
    static uint8_t tapeCurrentByte; 

    static uint8_t TAP_Play();
    
};

#endif
