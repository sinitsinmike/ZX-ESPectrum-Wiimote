#ifndef FileZ80_h
#define FileZ80_h

#include <Arduino.h>
#include <FS.h>

class FileZ80
{
public:
    static bool IRAM_ATTR load(String z80_fn);
    // static bool IRAM_ATTR save(String z80_fn);

private:
    static void loadCompressedMemData(File f, uint16_t dataLen, uint16_t memStart, uint16_t memlen);
    static void loadCompressedMemPage(File f, uint16_t dataLen, uint8_t* memPage, uint16_t memlen);
};

#endif