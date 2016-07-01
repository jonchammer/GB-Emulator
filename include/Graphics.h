#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Common.h"
#include "EmulatorConfiguration.h"
#include "Memory.h"
#include "Emulator.h"

class Memory;
class Emulator;

const int SPRITE_CLOCKS[] = {0, 8, 20, 32, 44, 52, 64, 76, 88, 96, 108};

#define LCD_ON() (((mLCDC) & 0x80) != 0)
#define GET_LCD_MODE() ((GameboyLCDModes)(mSTAT & 0x3))
#define SET_LCD_MODE(value) {mSTAT &= 0xFC; mSTAT |= (value);}
#define GET_TILE_PIXEL(VRAMAddr, x, y) ((((mVRAM[(VRAMAddr) + (y) * 2 + 1] >> (7 - (x))) & 0x1) << 1) | ((mVRAM[(VRAMAddr) + (y) * 2] >> (7 - (x))) & 0x1))

class Graphics : public Component
{
public:
    
    const static int VRAM_BANK_SIZE = 0x2000;
    
    // Platform independent memory addresses
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
    const static word WY         = 0xFF4A;
    const static word WX         = 0xFF4B;
    
    // LCD controller modes in the STAT register
    enum GameboyLCDModes
    {
        GBLCDMODE_HBLANK = 0, // H-Blank period. CPU can access the VRAM and OAM
                              // Lasts for (204 - (SCX % 8) - Sprites) cycles
                              // Here LCD controller compensates mode 3 delays so a scanline would take exactly 456 cycles

        GBLCDMODE_VBLANK = 1, // V-Blank period. CPU can access the VRAM and OAM
                
        GBLCDMODE_OAM    = 2, // OAM is being used ($FE00-$FE9F). CPU cannot access the OAM during this period

        GBLCDMODE_OAMRAM = 3  // Both the OAM and VRAM are being used. The CPU cannot access either during this period
                              // This is where all rendering is done. Lasts for (172 + (SCX % 8) + Sprites) cycles
                              // Sprites causing this mode to take longer
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
    Graphics(Memory* memory, EmulatorConfiguration* configuration);
    virtual ~Graphics();
    
    // State transitions
    virtual void reset();
    virtual void update(int clockDelta) = 0;
    
    // Screen information
    byte* getScreenData()  { return mFrontBuffer; }
    
    /**
     * Returns a 32x32 tile (256x256 px) color image that represents the
     * current background map. This is a direct representation of the current
     * state of the VRAM on the device.
     */
    virtual byte* getBackgroundMap(bool printGrid = true) = 0;
    
    // Debug
    void toggleBackground() { mBackgroundGlobalToggle = !mBackgroundGlobalToggle; }
    void toggleWindow()     { mWindowsGlobalToggle    = !mWindowsGlobalToggle;    }
    void toggleSprites()    { mSpritesGlobalToggle    = !mSpritesGlobalToggle;    }
    virtual byte* getVRAM(int& vramSize) = 0;
    bool dumpVRAM(const string& filename);
    
protected:
    
    // Important pointers to other components
    Memory* mMemory;
    EmulatorConfiguration* mConfig;
    
    // Screen buffers
    byte* mFrontBuffer;            // The pixel data that is currently being shown on the screen
    byte* mBackBuffer;             // The pixel data that is currently being written
    byte* mNativeBuffer;           // Holds pixel colors in palette values
    
    // Common storage
    std::vector<int> mSpriteQueue; // Contains sprites to be rendered in the right order
    byte mOAM[0xA0];
    
     // LCD modes loop
    InternalLCDModes mLCDMode;
    int mClockCounter;
    int mClocksToNextState;    // Delay before switching LCD mode
    int mScrollXClocks;        // Delay for the SCX Gameboy bug
    bool mLCDCInterrupted;     // Makes sure that only one LCDC Interrupt gets requested per scanline
    byte mWindowLine;
    
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
    
    // LCDC Y-Coordinate (R)
    // The LY indicates the vertical line to which the present data is transferred to the LCD
    // Driver. The LY can take on any value between 0 through 153. The values between
    // 144 and 153 indicate the V-Blank period. Writing will reset the counter
    byte mLY; 
    
    // LY Compare (R/W)
    // The LYC compares itself with the LY. If the values are the same it causes the STAT to set the coincident flag
    byte mLYC; 
    
    // Scroll Y (R/W)
    // 8 Bit value $00-$FF to scroll BG Y screen position
    byte mSCY; 

    // Scroll X (R/W)
    // 8 Bit value $00-$FF to scroll BG X screen position
    byte mSCX; 
    
    // Window Y Position (R/W)
    // 0 <= WY <= 143
    // WY must be greater than or equal to 0 and must be less than or equal to 143 for window to be visible
    byte mWY; 

    // Window X Position (R/W) - 7
    // 0 <= WX <= 166
    // WX must be greater than or equal to 0 and must be less than or equal to 166 for window to be visible
    byte mWX; 
    
    // OAM DMA information
    bool mOAMDMAStarted;
    word mOAMDMAProgress;
    word mOAMDMASource;
    int mOAMDMAClockCounter;
    
    // Debug
    bool mBackgroundGlobalToggle;
    bool mWindowsGlobalToggle;
    bool mSpritesGlobalToggle;
    
    // Common functions
    void writeOAM(byte addr, byte value);
    byte readOAM(byte addr) const;
    void LCDCChanged(byte value);
    void STATChanged(byte value);
    void LYCChanged(byte value);
    void prepareSpriteQueue();
    void checkCoincidenceFlag();
    void DMAStep(int clockDelta);
    void DMAChanged(byte value); 
};

#endif