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


#endif // FileUtils_h