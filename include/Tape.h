#include <FS.h>

#ifndef Tape_h
#define Tape_h

// Tape status definitions
#define TAPE_IDLE 0
#define TAPE_LOADING 1
#define TAPE_SAVING 2

// Tape sync phases lenght in microseconds
#define TAPE_SYNC_LEN 620   // microseconds for 2168 tStates (48K)
#define TAPE_SYNC1_LEN 190  // microseconds for 667 tStates (48K)
#define TAPE_SYNC2_LEN 210  // microseconds for 735 tStates (48K)

#define TAPE_HDR_LONG 8063   // Header sync lenght in pulses
#define TAPE_HDR_SHORT 3223  // Data sync lenght in pulses

#define TAPE_BIT0_PULSELEN 244 // Lenght of pulse for bit 1
#define TAPE_BIT1_PULSELEN 488 // Lenght of pulse for bit 0

#define TAPE_BLK_PAUSELEN 500000UL // 1/2 second of pause between blocks

class Tape
{
public:

    // Tape
    static String tapeFileName;
    static uint8_t tapeStatus;
    static uint8_t tapePhase;
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
    static uint8_t TAP_Read();
    
};

#endif
