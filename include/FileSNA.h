#ifndef FileSNA_h
#define FileSNA_h

#include <Arduino.h>

class FileSNA
{
public:
    static bool IRAM_ATTR load(String sna_fn);
    static bool IRAM_ATTR save(String sna_fn);

    static bool IRAM_ATTR loadQuick();
    static bool IRAM_ATTR saveQuick();

    static bool IRAM_ATTR isQuickAvailable();
    static bool IRAM_ATTR isPersistAvailable();
};

#endif