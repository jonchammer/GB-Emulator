/* 
 * File:   EmulatorConfiguration.h
 * Author: Jon
 *
 * Created on June 24, 2016, 12:00 PM
 */

#include "Common.h"

#ifndef EMULATORCONFIGURATION_H
#define EMULATORCONFIGURATION_H

struct EmulatorConfiguration
{
    System system;                // Which hardware we are emulating. When AUTOMATIC is chosen,
                                  // this will be changed to one of the other values when the
                                  // cartridge is loaded.
    int soundSampleRate;          // Sample rate for synthesized sound
    int soundSampleBufferLength;  // The size of the sound system's sample buffer (in samples)
    bool skipBIOS;                // When true, the BIOS screen will be skipped
    GameboyPalette gbPalette;     // Adjust the colors of Gameboy games
    double gbcDisplayGamma;       // Gamma correction applied to lighten/darken the screen. 
                                  // [0, infinity) or DISPLAY_GAMMA_DEFAULT.
    double gbcDisplaySaturation;  // [0, 1] - 0 is black/white, 1 is full color
                                  // Can also be DISPLAY_SATURATION_DEFAULT.
    bool doubleSpeed;             // When true, GBC double speed mode is activated. This should
                                  // not be set by the user. It will be used internally.
    
    // Default constructor initializes each field to reasonable default values
    EmulatorConfiguration() : 
        system(System::AUTOMATIC), 
        soundSampleRate(44100), 
        soundSampleBufferLength(1024),
        skipBIOS(false),
        gbPalette(GameboyPalette::GRAYSCALE), 
        gbcDisplayGamma(DISPLAY_GAMMA_DEFAULT),
        gbcDisplaySaturation(DISPLAY_SATURATION_DEFAULT),
        doubleSpeed(false) {}
};

#endif /* EMULATORCONFIGURATION_H */

