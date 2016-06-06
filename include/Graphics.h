#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Common.h"

const int SPRITE_CLOCKS[] = {0, 8, 20, 32, 44, 52, 64, 76, 88, 96, 108};

#define LCD_ON() (((mLCDC) & 0x80) != 0)
#define GET_LCD_MODE() ((GameboyLCDModes)(mSTAT & 0x3))
#define SET_LCD_MODE(value) {mSTAT &= 0xFC; mSTAT |= (value);}
#define GET_TILE_PIXEL(VRAMAddr, x, y) ((((mVRAM[(VRAMAddr) + (y) * 2 + 1] >> (7 - (x))) & 0x1) << 1) | ((mVRAM[(VRAMAddr) + (y) * 2] >> (7 - (x))) & 0x1))

class Graphics : public Component
{
public:
    
    const static int VRAMBankSize = 0x2000;
    
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
    
    Graphics() : mBackgroundGlobalToggle(true), mWindowsGlobalToggle(true), mSpritesGlobalToggle(true) {}
    
    virtual ~Graphics() {}
    virtual void reset() = 0;
    virtual void update(int clockDelta) = 0;
    virtual byte* getScreenData() = 0;
    
    // Debug
    void toggleBackground() { mBackgroundGlobalToggle = !mBackgroundGlobalToggle; }
    void toggleWindow()     { mWindowsGlobalToggle    = !mWindowsGlobalToggle;    }
    void toggleSprites()    { mSpritesGlobalToggle    = !mSpritesGlobalToggle;    }
    
protected:
    
    // Debug
    bool mBackgroundGlobalToggle;
    bool mWindowsGlobalToggle;
    bool mSpritesGlobalToggle;
};

#endif