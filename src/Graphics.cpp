/* 
 * File:   Graphics.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 10:12 PM
 */

#include "Graphics.h"
#include <cstdlib>

Graphics::Graphics(Memory* memory) : mMemory(memory), mScanlineCounter(0)
{
    mScreenData = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4];
}

Graphics::~Graphics()
{
    delete[] mScreenData;
    mScreenData = NULL;
}

void Graphics::reset()
{
    mScanlineCounter = 0;
    for (int i = 0; i < SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4; ++i)
        mScreenData[i] = 0x00;
}

void Graphics::update(Emulator* emulator, int cycles)
{
    setLCDStatus(emulator);
    if (!isLCDEnabled()) return;    
    
    mScanlineCounter -= cycles;

    // The current scanline is updated every 456 cycles. 
    if (mScanlineCounter <= 0)
    {
        // It's time to move on to the next scanline
        mMemory->writeNaive(SCANLINE_ADDRESS, mMemory->read(SCANLINE_ADDRESS) + 1);
        byte currentLine = mMemory->read(SCANLINE_ADDRESS);
        
        // Reset the counter
        mScanlineCounter = 456;
        
        // We have entered the vertical blank period. Send off the interrupt.
        if (currentLine == SCREEN_HEIGHT_PIXELS)
            mMemory->requestInterrupt(INTERRUPT_VBLANK);
        
        // There are 8 vitual scanlines. This gives the game a chance to access
        // the video memory.
        // If we've gone past scanline 153, reset to 0. (Actually set it to 0xFF
        // because the first thing we do the next iteration is increment it. This
        // causes the value to roll over to 0, so the first scanline actually gets
        // rendered.)
        
        // TODO: I think it should reset to 0 when currentLine is >= 152, but
        // I haven't been able to find support for it yet.
        else if (currentLine > 153)
            mMemory->writeNaive(SCANLINE_ADDRESS, -1);
        
        // Draw the current scanline
        else if (currentLine < SCREEN_HEIGHT_PIXELS)
            drawScanLine();
    }
}

void Graphics::setLCDStatus(Emulator* emulator)
{
    byte status = mMemory->read(LCD_STATUS_ADDRESS);
    if (!isLCDEnabled())
    {
        // Set the mode to 1 during LCD disabled and reset the scanline
        mScanlineCounter = 456;
        mMemory->writeNaive(SCANLINE_ADDRESS, 0);
        status &= 0xFC;
        status = setBit(status, 0);
        mMemory->write(LCD_STATUS_ADDRESS, status);
        return;
    }
    
    byte currentLine = mMemory->read(SCANLINE_ADDRESS);
    byte currentMode = status & 0x3;
    
    byte mode             = 0;
    bool requestInterrupt = false;
    
    // In Vblank, so set mode to 1
    if (currentLine >= SCREEN_HEIGHT_PIXELS)
    {
        mode             = 1;
        status           = setBit(status, 0);
        status           = clearBit(status, 1);
        requestInterrupt = testBit(status, 4);
    }
    
    else
    {
        // Mode 2 takes 80/456 cycles. Mode 3 takes 172/456 cycles.
        int mode2Bounds = 456 - 80;
        int mode3Bounds = mode2Bounds - 172;
        
        // Mode 2
        if (mScanlineCounter >= mode2Bounds)
        {
            mode             = 2;
            status           = setBit(status, 1);
            status           = clearBit(status, 0);
            requestInterrupt = testBit(status, 5);
        }
        
        // Mode 3
        else if (mScanlineCounter >= mode3Bounds)
        {
            mode   = 3;
            status = setBit(status, 1);
            status = setBit(status, 0);
            // No interrupt for entering Mode 3
        }
        
        // Mode 0
        else
        {
            mode             = 0;
            status           = clearBit(status, 1);
            status           = clearBit(status, 0);
            requestInterrupt = testBit(status, 3);
        }
    }
    
    // When we enter a new mode, we can request an interrupt
    // to let the client know about the mode change
    if (requestInterrupt && (mode != currentMode))
        mMemory->requestInterrupt(INTERRUPT_LCD);
    
    // Set the coincidence flag, if applicable
    if (currentLine == mMemory->read(COINCIDENCE_ADDRESS))
    {
        status = setBit(status, 2);
        if (testBit(status, 6))
            mMemory->requestInterrupt(INTERRUPT_LCD);
    }
    else status = clearBit(status, 2);
    
    // Save the final status to the proper location in memory
    mMemory->write(LCD_STATUS_ADDRESS, status);
}

bool Graphics::isLCDEnabled()
{
    return testBit(mMemory->read(LCD_CONTROL_REGISTER), 7);
}

void Graphics::drawScanLine()
{     
    byte control = mMemory->read(LCD_CONTROL_REGISTER);
    if (testBit(control, 0))
        renderTiles();
    
    if (testBit(control, 1))
        renderSprites();
}

void Graphics::renderTiles()
{
    byte lcdControl       = mMemory->read(LCD_CONTROL_REGISTER);
    word tileData         = 0;
    word backgroundMemory = 0;
    bool unsign           = true;
    
    // Where to draw the visual area and the window
    byte scrollX = mMemory->read(SCROLL_X);
    byte scrollY = mMemory->read(SCROLL_Y);
    byte windowX = mMemory->read(WINDOW_X) - 7;
    byte windowY = mMemory->read(WINDOW_Y);
    
    bool usingWindow = false;
    
    // Is the window enabled?
    if (testBit(lcdControl, 5))
    {
        // Is the current scanline we're drawing within the window's y pos?
        if (windowY <= mMemory->read(SCANLINE_ADDRESS))
            usingWindow = true;
    }
    
    // Which tile data are we using?
    if (testBit(lcdControl, 4))
    {
        tileData = 0x8000;
    }
    else
    {
        // IMPORTANT: This memory region uses signed bytes as tile identifiers
        tileData = 0x8800;
        unsign   = false;
    }
    
    // Which background memory?
    if (!usingWindow)
    {
        if (testBit(lcdControl, 3))
            backgroundMemory = 0x9C00;
        else backgroundMemory = 0x9800;
    }
    else
    {
        // Which window memory?
        if (testBit(lcdControl, 6))
            backgroundMemory = 0x9C00;
        else backgroundMemory = 0x9800;
    }
    
    byte yPos = 0;
    
    // yPos is used to calculate which of 32 vertical tiles the
    // current scanline is drawing
    if (!usingWindow)
        yPos = scrollY + mMemory->read(SCANLINE_ADDRESS);
    else
        yPos = mMemory->read(SCANLINE_ADDRESS) - windowY;
    
    // Which of the 8 vertical pixels of the current tile is the scanline on?
    word tileRow = (((byte) (yPos / 8)) * 32);
    
    // Time to start drawing the 160 horizontal pixels for this scanline
    for (int pixel = 0; pixel < 160; ++pixel)
    {
        byte xPos = pixel;
        if (!usingWindow)
            xPos += scrollX;
        
        // Which of the 32 horizontal tiles does this xPos fall within?
        word tileCol = (xPos / 8);
        signedWord tileNum;
        
        // Get the tile identity number. Remember, it can be signed or unsigned.
        word tileAddress = backgroundMemory + tileRow + tileCol;
        if (unsign)
            tileNum = (byte) mMemory->read(tileAddress);
        else
            tileNum = (signedByte) mMemory->read(tileAddress);
        
        // Deduce where this tile identifier is in memory.
        word tileLocation = tileData;
        if (unsign)
            tileLocation += (tileNum * 16);
        else
            tileLocation += ((tileNum + 128) * 16);
        
        // Find the correct vertical line we're on of the tile to get the tile data from memory
        byte line = yPos % 8;
        line *= 2; // Each vertical line takes up two bytes of memory
        byte data1 = mMemory->read(tileLocation + line);
        byte data2 = mMemory->read(tileLocation + line + 1);
        
        // Calculate color bit
        int colorBit = 7 - xPos % 8;
        
        // Combine data 2 and data 1 to get the color ID for this pixel in the tile
        int colorNum = getBit(data2, colorBit);
        colorNum <<= 1;
        colorNum |= getBit(data1, colorBit);
        
        // Now that we have the color ID, get the actual color from the palette
        int color  = getColor(colorNum, BACKGROUND_PALETTE_ADDRESS);
        int yCoord = mMemory->read(SCANLINE_ADDRESS);
        if ((yCoord < 0) || (yCoord > 143) || (pixel < 0) || (pixel > 159))
            continue;
        
        // Set the data
        int index = 4 * (yCoord * SCREEN_WIDTH_PIXELS + pixel);
        mScreenData[index]     = (color >> 16) & 0xFF; // Red
        mScreenData[index + 1] = (color >>  8) & 0xFF; // Green
        mScreenData[index + 2] = (color      ) & 0xFF; // Blue
        mScreenData[index + 3] = 0xFF;                 // Alpha
    }
}

void Graphics::renderSprites()
{
    // Sort out whether our sprites are 8x8 or 8x16
    int ySize = 8;
    if (testBit(mMemory->read(LCD_CONTROL_REGISTER), 2))
        ySize = 16;
    
    int scanline = mMemory->read(SCANLINE_ADDRESS);
    
    for (int sprite = 0; sprite < 40; ++sprite)
    {
        // Sprite occupies 4 bytes in the sprite attributes table
        byte index        = sprite * 4;
        int yPos          = mMemory->read(0xFE00 + index) - 16;
        int xPos          = mMemory->read(0xFE00 + index + 1) - 8;
        byte tileLocation = mMemory->read(0xFE00 + index + 2);
        byte attributes   = mMemory->read(0xFE00 + index + 3);
        
        bool xFlip = testBit(attributes, 5);
        bool yFlip = testBit(attributes, 6);
        
        // Is this sprite drawn on this scanline?
        if ((scanline >= yPos) && (scanline < (yPos + ySize)))
        {
            int line = scanline - yPos;
            
            // Read the sprite in backwards in the y axis
            if (yFlip)
            {
                // 0 -> 15, 1 -> 14, etc.
                line = ySize - 1 - line;
            }
            
            line *= 2; // Same as for tiles
            word dataAddress = (0x8000 + (tileLocation * 16)) + line;
            byte data1 = mMemory->read(dataAddress);
            byte data2 = mMemory->read(dataAddress + 1);
            
            // It's easier to read in from right to left as pixel 0 is
            // bit 7 in the color data, pixel 1 is bit 6, etc.
            for (int tilePixel = 7; tilePixel >= 0; tilePixel--)
            {
                int colorBit = tilePixel;
                
                // Read the sprite in backwards for the x axis
                if (xFlip)
                    colorBit = 7 - colorBit;
                
                // Same as for tiles
                int colorNum = getBit(data2, colorBit);
                colorNum <<= 1;
                colorNum |= getBit(data1, colorBit);
                
                // White is transparent for sprites
                if (colorNum == 0x00) continue;
                
                word colorAddress = testBit(attributes, 4) ? SPRITE_PALETTE_ADDRESS2 : SPRITE_PALETTE_ADDRESS1;
                int color         = getColor(colorNum, colorAddress);
                
                int pixel = xPos + 7 - tilePixel;
                
                // Sanity check
                if ((scanline < 0) || (scanline > 143) || (pixel < 0) || (pixel > 159)) continue;
                
                int index = 4 * (scanline * SCREEN_WIDTH_PIXELS + pixel);
                
                // Check if pixel is hidden behind background
                if (testBit(attributes, 7))
                {
                    // This sprite is rendered underneath the background unless
                    // the background is white
                    if ((mScreenData[index] != 0xFF) || (mScreenData[index + 1] != 0xFF) || (mScreenData[index + 2] != 0xFF))
                        continue;
                }
                
                // Draw this pixel
                mScreenData[index]     = (color >> 16) & 0xFF; // Red
                mScreenData[index + 1] = (color >>  8) & 0xFF; // Green
                mScreenData[index + 2] = (color      ) & 0xFF; // Blue
                mScreenData[index + 3] = 0xFF;                 // Alpha
            }
        }
    }
}

int Graphics::getColor(byte colorNum, word address) const
{
    int result   = WHITE;
    byte palette = mMemory->read(address);
    int hi       = 0;
    int lo       = 0;
    
    // Which bits of the color palette does the color ID map to?
    switch (colorNum)
    {
        case 0x0: hi = 1; lo = 0; break;
        case 0x1: hi = 3; lo = 2; break;
        case 0x2: hi = 5; lo = 4; break;
        case 0x3: hi = 7; lo = 6; break;
        
        default: 
        {
            cerr << "Color code not recognized: ";
            printHex(cerr, colorNum);
            cerr << ". See getColor()." << endl;
        }
    }
    
    // Use the color palette to get the color
    int color = getBit(palette, hi) << 1;
    color    |= getBit(palette, lo);
    
    switch (color)
    {
        case 0x0: result = WHITE;      break;
        case 0x1: result = LIGHT_GRAY; break;
        case 0x2: result = DARK_GRAY;  break;
        case 0x3: result = BLACK;      break;
        
        default: cerr << "Color " << color << " not defined! Check Graphics::getColor()" << endl;
    }
    
    return result;
}
