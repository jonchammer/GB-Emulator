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

    GBCGraphics(Memory* memory, EmulatorConfiguration* configuration);
    virtual ~GBCGraphics();
    
    // State transitions
    void update(int clockDelta);
    void reset();

    // Memory access
    byte read(const word address) const;
    void write(const word address, const byte data);
    
    // Screen information
    byte* getScreenData()  { return mFrontBuffer; }
    
private:
    
    Memory* mMemory;
    EmulatorConfiguration* mConfig;
    
    byte mVRAM[VRAMBankSize * 2];
    word mVRAMBankOffset;
    byte mOAM[0xA0];
    int mGBC2RGBPalette[32768];    // GBG color -> ARGB color
    
    byte* mFrontBuffer;            // The pixel data that is currently being shown on the screen
    byte* mBackBuffer;             // The pixel data that is currently being written
    byte* mNativeBuffer;           // Holds pixel colors in palette values
    std::vector<int> mSpriteQueue; // Contains sprites to be rendered in the right order
    
    // LCD modes loop
    int mClockCounter;
    int mClocksToNextState;    // Delay before switching LCD mode
    int mScrollXClocks;        // Delay for the SCX Gameboy bug
    bool mLCDCInterrupted;     // Makes sure that only one LCDC Interrupt gets requested per scanline
    InternalLCDModes mLCDMode;

    byte mWindowLine;
    int mDelayedWY;
    
    // GPU I/O ports
    
    // LCD Control (R/W)
    // Bit 7 - LCD Control Operation
    //	0: Stop completely (no picture on screen)
    //	1: operation
    // Bit 6 - Window Tile Map Display Select
    //	0: $9800-$9BFF
    //	1: $9C00-$9FFF
    // Bit 5 - Window Display
    //	0: off
    //	1: on
    // Bit 4 - BG & Window Tile Data Select
    //	0: $8800-$97FF
    //	1: $8000-$8FFF <- Same area as OBJ
    // Bit 3 - BG Tile Map Display Select
    //	0: $9800-$9BFF
    //	1: $9C00-$9FFF
    // Bit 2 - OBJ (Sprite) Size
    //	0: 8*8
    //	1: 8*16 (width*height)
    // Bit 1 - OBJ (Sprite) Display
    //	0: off
    //	1: on
    // Bit 0 - BG & Window Display
    //	0: off
    //	1: on
    byte mLCDC; 
    
    // LCDC Status (R/W)
    // Bits 6-3 - Interrupt Selection By LCDC Status
    // Bit 6 - LYC=LY Coincidence (Selectable)
    // Bit 5 - Mode 10
    // Bit 4 - Mode 01
    // Bit 3 - Mode 00
    //	    0: Non Selection
    //	    1: Selection
    // Bit 2 - Coincidence Flag
    //	    0: LYC not equal to LCDC LY
    //	    1: LYC = LCDC LY
    // Bit 1-0 - Mode Flag
    //     00: During H-Blank
    // 	   01: During V-Blank
    // 	   10: During Searching OAM-RAM
    //     11: During Transfering Data to LCD Driver
    byte mSTAT; 

    // Scroll Y (R/W)
    // 8 Bit value $00-$FF to scroll BG Y screen position
    byte mSCY; 

    // Scroll X (R/W)
    // 8 Bit value $00-$FF to scroll BG X screen position
    byte mSCX; 

    // LCDC Y-Coordinate (R)
    // The LY indicates the vertical line to which the present data is transferred to the LCD
    // Driver. The LY can take on any value between 0 through 153. The values between
    // 144 and 153 indicate the V-Blank period. Writing will reset the counter
    byte mLY; 

    // LY Compare (R/W)
    // The LYC compares itself with the LY. If the values are the same it causes the STAT to set the coincident flag
    byte mLYC; 

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

    // Window Y Position (R/W)
    // 0 <= WY <= 143
    // WY must be greater than or equal to 0 and must be less than or equal to 143 for window to be visible
    byte mWY; 

    // Window X Position (R/W)
    // 0 <= WX <= 166
    // WX must be greater than or equal to 0 and must be less than or equal to 166 for window to be visible
    byte mWX; 
    
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
    
    // OAM DMA
    bool mOAMDMAStarted;
    word mOAMDMAProgress;
    word mOAMDMASource;
    int mOAMDMAClockCounter;

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
    void prepareSpriteQueue();
    void checkLYC();
    bool HDMACopyBlock(word source, word dest);
    
    // Getters / Setters for memory access
    void writeVRAM(word addr, byte value);
    byte readVRAM(word addr) const;
    void writeOAM(byte addr, byte value);
    byte readOAM(byte addr) const;
    void DMAStep(int clockDelta);
    void DMAChanged(byte value);
    void LCDCChanged(byte value);
    void STATChanged(byte value);
    void LYCChanged(byte value);
    
    void VBKChanged(byte value);
    void HDMA1Changed(byte value);
    void HDMA2Changed(byte value);
    void HDMA3Changed(byte value);
    void HDMA4Changed(byte value);
    void HDMA5Changed(byte value);
    void BGPIChanged(byte value)
    {
        mBGPI = value;
    }
    void BGPDChanged(byte value);

    void OBPIChanged(byte value)
    {
        mOBPI = value;
    }
    void OBPDChanged(byte value);
};

#endif /* GBCGRAPHICS_H */

