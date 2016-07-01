/* 
 * File:   GBGraphics.h
 * Author: Jon C. Hammer
 *
 * Created on June 5, 2016, 9:33 PM
 */

#ifndef GBGRAPHICS_H
#define GBGRAPHICS_H

#include <vector>
#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <cstring>
#include "Common.h"
#include "Memory.h"
#include "Emulator.h"

class Memory;
class Emulator;
   
class GBGraphics : public Graphics
{
public:

    // GB-specific memory addresses
    const static word BGP  = 0xFF47;
    const static word OBP0 = 0xFF48;
    const static word OBP1 = 0xFF49;
    
    // Constructors / Destructors
    GBGraphics(Memory* memory, EmulatorConfiguration* configuration);
    
    // State transitions
    void update(int clockDelta);
    void reset();

    // Memory access
    byte read(const word address) const;
    void write(const word address, const byte data);
    
    byte* getBackgroundMap(bool printGrid);
    byte* getVRAM(int& vramSize) {vramSize = VRAM_BANK_SIZE; return mVRAM;}
    
private:
    //GPU memory
    byte mVRAM[VRAM_BANK_SIZE];
    int mGB2RGBPalette[4];         // Gameboy palette -> ARGB color
    
    // GPU I/O ports

    // BG & Window Palette Data (R/W)
    // Bit 7-6 - Data for Dot Data 11 (Normally darkest color)
    // Bit 5-4 - Data for Dot Data 10
    // Bit 3-2 - Data for Dot Data 01
    // Bit 1-0 - Data for Dot Data 00 (Normally lightest color)
    byte mBGP; 

    // Object Palette 0 Data (R/W)
    // This selects the colors for sprite palette 0. It works exactly as BGP except each value of 0 is transparent
    byte mOBP0; 

    // Object Palette 1 Data (R/W)
    // This selects the colors for sprite palette 1. It works exactly as OBP0
    byte mOBP1; 

    // GPU Microcode
    void renderScanline();
    void renderBackground();
    void renderWindow();
    void renderSprites();
       
    // Getters / Setters for memory access
    void writeVRAM(word addr, byte value);
    byte readVRAM(word addr) const;  
};

#endif /* GBGRAPHICS_H */

