#ifndef AySound_h
#define AySound_h

#include "hardconfig.h"

class AySound
{
public:
#ifndef USE_AY_SOUND
    static void initialize() {}
    static void update() {}
    static void resetSound() {}
    static void silenceAllChannels(bool flag) {}
    static uint8_t getRegisterData() { return 0; }
    static void selectRegister(uint8_t data) {}
    static void setRegisterData(uint8_t data) {}
#else
    static void initialize();

    static void update();

    static void resetSound();
    static void silenceAllChannels(bool silence);

    static uint8_t getRegisterData();
    static void selectRegister(uint8_t data);
    static void setRegisterData(uint8_t data);

private:
    static uint8_t finePitchChannelA;
    static uint8_t coarsePitchChannelA;
    static uint8_t finePitchChannelB;
    static uint8_t coarsePitchChannelB;
    static uint8_t finePitchChannelC;
    static uint8_t coarsePitchChannelC;
    static uint8_t noisePitch;
    static uint8_t mixer;
    static uint8_t volumeChannelA;
    static uint8_t volumeChannelB;
    static uint8_t volumeChannelC;
    static uint8_t envelopeFineDuration;
    static uint8_t envelopeCoarseDuration;
    static uint8_t envelopeShape;
    static uint8_t ioPortA;

    // Status
    static uint8_t selectedRegister;
    static uint8_t channelVolume[3];
    static uint16_t channelFrequency[3];
#endif
};

#endif // AySound_h