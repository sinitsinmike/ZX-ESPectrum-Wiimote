#ifndef FileUtils_h
#define FileUtils_h

// Defines
#define ASCII_NL 10
#define ON true
#define OFF false

// Headers
#include <Arduino.h>
#include <FS.h>

class FileUtils
{
public:
    static void IRAM_ATTR initFileSystem();
    static String         getAllFilesFrom(const String path);
    static void           listAllFiles();
    static void           sanitizeFilename(String filename); // in-place
    static File IRAM_ATTR safeOpenFileRead(String filename);
    static String         getFileEntriesFromDir(String path);
    static uint16_t       countFileEntriesFromDir(String path);
    static String         getSortedSnaFileList();
    static String         getSnaFileList();

    static bool           hasSNAextension(String filename);
    static bool           hasZ80extension(String filename);

private:
    friend class          Config;
    static void           loadRom(String arch, String romset);
};

#define DISK_BOOT_FILENAME "/boot.cfg"
#define DISK_ROM_DIR "/rom"
#define DISK_SNA_DIR "/sna"
#define DISK_PSNA_FILE "/persist/persist.sna"
#define NO_RAM_FILE "none"
#define SNA_48K_SIZE 49179


// inline utility functions for uniform access to file/memory
// and making it easy to to implement SNA/Z80 functions

static inline uint8_t readByteFile(File f)
{
    return f.read();
}

static inline uint16_t readWordFileLE(File f)
{
    uint8_t lo = f.read();
    uint8_t hi = f.read();
    return lo | (hi << 8);
}

static inline uint16_t readWordFileBE(File f)
{
    uint8_t hi = f.read();
    uint8_t lo = f.read();
    return lo | (hi << 8);
}

static inline void writeByteFile(uint8_t value, File f)
{
    f.write(value);
}

static inline void writeWordFileLE(uint16_t value, File f)
{
    uint8_t lo =  value       & 0xFF;
    uint8_t hi = (value >> 8) & 0xFF;
    f.write(lo);
    f.write(hi);
}

static inline void writeWordFileBE(uint16_t value, File f)
{
    uint8_t hi = (value >> 8) & 0xFF;
    uint8_t lo =  value       & 0xFF;
    f.write(hi);
    f.write(lo);
}

static inline uint8_t readByteMem(uint8_t*& ptr)
{
    uint8_t value = *ptr++;
    return value;
}

static inline uint16_t readWordMemLE(uint8_t*& ptr)
{
    uint8_t lo = *ptr++;
    uint8_t hi = *ptr++;
    return lo | (hi << 8);
}

static inline uint16_t readWordMemBE(uint8_t*& ptr)
{
    uint8_t hi = *ptr++;
    uint8_t lo = *ptr++;
    return lo | (hi << 8);
}

static inline void writeByteMem(uint8_t value, uint8_t*& ptr)
{
    *ptr++ = value;
}

static inline void writeWordMemLE(uint16_t value, uint8_t*& ptr)
{
    uint8_t lo =  value       & 0xFF;
    uint8_t hi = (value >> 8) & 0xFF;
    *ptr++ = lo;
    *ptr++ = hi;
}

static inline void writeWordMemBE(uint16_t value, uint8_t*& ptr)
{
    uint8_t hi = (value >> 8) & 0xFF;
    uint8_t lo =  value       & 0xFF;
    *ptr++ = hi;
    *ptr++ = lo;
}



#endif // FileUtils_h