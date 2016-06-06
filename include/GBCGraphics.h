/* 
 * File:   GBCGraphics.h
 * Author: Jon C. Hammer
 *
 * Created on June 5, 2016, 10:11 PM
 */

#ifndef GBCGRAPHICS_H
#define GBCGRAPHICS_H

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

class GBCGraphics : public Graphics
{
public:

    // Constructors / Destructors
    GBCGraphics(Memory* memory, EmulatorConfiguration* configuration);
    
    // State transitions
    void update(int clockDelta);
    void reset();

    // Memory access
    byte read(const word address) const;
    void write(const word address, const byte data);
    
private:
    
    byte mVRAM[VRAMBankSize * 2];
    word mVRAMBankOffset;
    int mGBC2RGBPalette[32768];    // GBG color -> ARGB color
    
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
    
    // Current Video Memory (VRAM) Bank
    // Bit 0 - VRAM Bank (0-1)
    byte mVBK; 

    // Background Palette Index
    // Bit 0-5   Index (00-3F)
    // Bit 7     Auto Increment  (0=Disabled, 1=Increment after Writing)
    byte mBGPI; 

    // Background Palette Data
    // Bit 0-4   Red Intensity   (00-1F)
    // Bit 5-9   Green Intensity (00-1F)
    // Bit 10-14 Blue Intensity  (00-1F)
    byte mBGPD[8 * 8]; 

    // Sprite Palette Index
    // Bit 0-5   Index (00-3F)
    // Bit 7     Auto Increment  (0=Disabled, 1=Increment after Writing)
    byte mOBPI; 

    // Sprite Palette Data
    // Bit 0-4   Red Intensity   (00-1F)
    // Bit 5-9   Green Intensity (00-1F)
    // Bit 10-14 Blue Intensity  (00-1F)
    byte mOBPD[8 * 8]; 
    
    // VRAM DMA
    bool mHDMAActive;
    word mHDMASource;
    word mHDMADestination;
    byte mHDMAControl; // New DMA Length/Mode/Start
    
    // GPU Microcode
    void renderScanline();
    void renderBackground();
    void renderWindow();
    void renderSprites();
    bool HDMACopyBlock(word source, word dest);
    
    // Getters / Setters for memory access
    void writeVRAM(word addr, byte value);
    byte readVRAM(word addr) const;
    
    void VBKChanged(byte value);
    void HDMA1Changed(byte value);
    void HDMA2Changed(byte value);
    void HDMA3Changed(byte value);
    void HDMA4Changed(byte value);
    void HDMA5Changed(byte value);
    void BGPDChanged(byte value);
    void OBPDChanged(byte value);
};

#endif /* GBCGRAPHICS_H */

