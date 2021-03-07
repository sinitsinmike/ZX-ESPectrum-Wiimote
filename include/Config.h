#ifndef Config_h
#define Config_h

#include <Arduino.h>

class Config
{
public:
    // machine type change request
    static void requestMachine(String newArch, String newRomSet, bool force);

    // config variables
    static const String& getArch()   { return arch;   }
    static const String& getRomSet() { return romSet; }
    static String   ram_file;
    static bool     slog_on;

    // config persistence
    static void           load();
    static void IRAM_ATTR save();

    // list of snapshot file names
    static String   sna_file_list;
    // list of snapshot display names
    static String   sna_name_list;

    // load lists of snapshots
    static void loadSnapshotLists();

private:
    static String   arch;
    static String   romSet;
};

#endif // Config.h