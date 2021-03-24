#ifndef FileSNA_h
#define FileSNA_h

#include <Arduino.h>

class FileSNA
{
public:
    static bool IRAM_ATTR load(String sna_fn);

    // save snapshot in SNA format to disk using specified filename.
    // this function tries to save using pages, 
    static bool IRAM_ATTR save(String sna_fn);
    // using this function you can choose whether to write pages, or byte by byte.
    static bool IRAM_ATTR save(String sna_fn, bool writePages);

    static bool IRAM_ATTR loadQuick();
    static bool IRAM_ATTR saveQuick();

    static bool IRAM_ATTR isQuickAvailable();
    static bool IRAM_ATTR isPersistAvailable();
};

#endif