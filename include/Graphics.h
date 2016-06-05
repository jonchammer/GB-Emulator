#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Common.h"
#include <vector>

class Memory;
class Emulator;

const int VRAMBankSize = 0x2000;

enum DMGPalettes
{
    RGBPALETTE_BLACKWHITE = 0,
    RGBPALETTE_REAL = 1
};
    
class Graphics : public Component
{
public:

    // Important memory locations
    const static word VRAM_START = 0x8000;
    const static word VRAM_END   = 0x9FFF;
    const static word OAM_START  = 0xFE00;
    const static word OAM_END    = 0xFE9F;
    const static word LCDC       = 0xFF40;
    const static word STAT       = 0xFF41;
    const static word SCY        = 0xFF42;
    const static word SCX        = 0xFF43;
    const static word LY         = 0xFF44;
    const static word LYC        = 0xFF45;
    const static word DMA        = 0xFF46;
    const static word BGP        = 0xFF47;
    const static word OBP0       = 0xFF48;
    const static word OBP1       = 0xFF49;
    const static word WY         = 0xFF4A;
    const static word WX         = 0xFF4B;
    const static word VBK        = 0xFF4F;
    const static word HDMA1      = 0xFF51;
    const static word HDMA2      = 0xFF52;
    const static word HDMA3      = 0xFF53;
    const static word HDMA4      = 0xFF54;
    const static word HDMA5      = 0xFF55;
    const static word BGPI       = 0xFF68;
    const static word BGPD       = 0xFF69;
    const static word OBPI       = 0xFF6A;
    const static word OBPD       = 0xFF6B;

    // LCD controller modes in the STAT register
    enum GameboyLCDModes
    {
        GBLCDMODE_OAM    = 2, // OAM is being used ($FE00-$FE9F). CPU cannot access the OAM during this period

        GBLCDMODE_OAMRAM = 3, // Both the OAM and VRAM are being used. The CPU cannot access either during this period
                              // This is where all rendering is done. Lasts for (172 + (SCX % 8) + Sprites) cycles
                              // Sprites causing this mode to take longer

        GBLCDMODE_HBLANK = 0, // H-Blank period. CPU can access the VRAM and OAM
                              // Lasts for (204 - (SCX % 8) - Sprites) cycles
                              // Here LCD controller compencates mode 3 delays so a scanline would take exactly 456 cycles

        GBLCDMODE_VBLANK = 1  // V-Blank period. CPU can access the VRAM and OAM
    };

    enum InternalLCDModes
    {
        LCDMODE_LY00_HBLANK,
        LCDMODE_LYXX_HBLANK,
        LCDMODE_LYXX_HBLANK_INC,
        LCDMODE_LY00_VBLANK,
        LCDMODE_LY9X_VBLANK,
        LCDMODE_LY9X_VBLANK_INC,
        LCDMODE_LYXX_OAM,
        LCDMODE_LYXX_OAMRAM
    };

    // Constructors / Destructors
    Graphics(Memory* memory, bool skipBIOS, const bool &_CGB, const bool &_CGBDoubleSpeed, DMGPalettes palette = RGBPALETTE_REAL);
    virtual ~Graphics();
    
    // State transitions
    void update(int clockDelta);
    void reset(bool skipBIOS);

    // Memory access
    byte read(const word address) const;
    void write(const word address, const byte data);
    
    // Screen information
    byte* getScreenData()  { return mFrontBuffer; }
    
    // Debug
    void toggleBackground() { mBackgroundGlobalToggle = !mBackgroundGlobalToggle; }
    void toggleWindow()     { mWindowsGlobalToggle    = !mWindowsGlobalToggle;    }
    void toggleSprites()    { mSpritesGlobalToggle    = !mSpritesGlobalToggle;    }

private:
    Memory* mMemory;
    
    // GPU Microcode
    void renderScanline();
    void renderBackground();
    void renderWindow();
    void renderSprites();
    void checkLYC();
    void prepareSpriteQueue();
    bool HDMACopyBlock(word source, word dest);
    int GBCColorToARGB(word color);

    const bool &mCGB;
    const bool &mCGBDoubleSpeed;

    //GPU memory
    byte mVRAM[VRAMBankSize * 2];
    byte mOAM[0xA0 + 0x5F];
    word mVRAMBankOffset;

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

    int mGB2RGBPalette[4];         // Gameboy palette -> ARGB color
    int mGBC2RGBPalette[32768];    // GBG color -> ARGB color
    int mSpriteClocks[11];         // Sprites rendering affects LCD timings
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

    // Debug
    bool mBackgroundGlobalToggle;
    bool mWindowsGlobalToggle;
    bool mSpritesGlobalToggle;
    
    // Getters / Setters for memory access
    void writeVRAM(word addr, byte value);
    void writeOAM(byte addr, byte value);
    byte readVRAM(word addr) const;
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
        if (mCGB)
        {
            mBGPI = value;
        }
    }
    void BGPDChanged(byte value);

    void OBPIChanged(byte value)
    {
        if (mCGB)
        {
            mOBPI = value;
        }
    }
    void OBPDChanged(byte value);
};

#endif