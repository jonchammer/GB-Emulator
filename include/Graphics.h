/* 
 * File:   Graphics.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 10:12 PM
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Common.h"

// Forward declaration of necessary classes
class Memory;
class Emulator;

class Graphics 
{
public:
    
    /**
     * Creates a new Graphics unit for the Gameboy.
     * 
     * @param memory. A pointer to the memory unit.
     */
    Graphics(Memory* memory);
    
    /**
     * Destructor.
     */
    ~Graphics();
    
    /**
     * Called during the emulation loop. Used to update the screen buffer
     * with the most recent contents.
     * 
     * @param cycles.   The number of cycles the last instruction took.
     */
    void update(int cycles);
    
    /**
     * Resets the state of the screen buffer to its initial configuration.
     */
    void reset();
    
    /**
     * Returns a pointer to the raw screen buffer. This can be used to draw
     * the screen onto an arbitrary surface. The format is RGBA, where each
     * component is one (unsigned) byte. The size is SCREEN_WIDTH_PIXELS *
     * SCREEN_HEIGHT_PIXELS * 4.
     */
    byte* getScreenData() { return mScreenData; }
    
private:
    Memory* mMemory;      // A pointer to the main memory unit.
    byte* mScreenData;    // Screen buffer (RGBA). Size is SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4.
    int mScanlineCounter; // Keeps track of when to move to the next scanline
    
    // Helper functions
    void setLCDStatus();
    bool isLCDEnabled();
    void drawScanLine();
    void renderTiles();
    void renderSprites();
    int getColor(byte colorNum, word address) const;
};

#endif /* GRAPHICS_H */

