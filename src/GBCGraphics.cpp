#include "GBCGraphics.h"
#include <cmath>

GBCGraphics::GBCGraphics(Memory* memory, EmulatorConfiguration* configuration) :
    Graphics(memory, configuration)
{
    // Attach this component to the memory at the correct locations
    mMemory->attachComponent(this, 0xFF4F, 0xFF4F); // VBK
    mMemory->attachComponent(this, 0xFF51, 0xFF55); // HDMA
    mMemory->attachComponent(this, 0xFF68, 0xFF6B); // GBC Registers
    mMemory->attachComponent(this, 0xFF57, 0xFF67); // ??? - Included by Gomeboy, but don't seem to be useful
    mMemory->attachComponent(this, 0xFF6C, 0xFF6F); // ???
        
    reset();
}

void GBCGraphics::update(int clockDelta)
{
	mClockCounter += clockDelta;
	
    // Take care of DMA transfers
	if (mOAMDMAStarted)
		DMAStep(clockDelta);

	while (mClockCounter >= mClocksToNextState)
	{
		mClockCounter -= mClocksToNextState;

		if (!LCD_ON())
		{
			mLY                = 0;
			mClocksToNextState = 70224;
            
            memset(mBackBuffer, 0xFF, SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4 * sizeof(mBackBuffer[0]));
            memset(mNativeBuffer, 0x00, SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * sizeof(mNativeBuffer[0]));
            std::swap(mBackBuffer, mFrontBuffer);
			continue;
		}
		
		switch (mLCDMode)
		{
            // Mode 2 - Lasts 80 cycles
            case LCDMODE_LYXX_OAM:
                SET_LCD_MODE(GBLCDMODE_OAM);

                // OAM interrupt selected
                if (testBit(mSTAT, 5) && !mLCDCInterrupted)
                {
                    mMemory->requestInterrupt(INTERRUPT_LCD);
                    mLCDCInterrupted = true;
                }

                // LYC=LY checked in the beginning of scanline
                checkCoincidenceFlag();

                mScrollXClocks     = (mSCX & 0x4) ? 4 : 0;
                mLCDMode           = LCDMODE_LYXX_OAMRAM;
                mClocksToNextState = 80;
                break;

            // Mode 3 - Lasts 172 cycles + a variable amount that depends on what's being rendered
            case LCDMODE_LYXX_OAMRAM:
                prepareSpriteQueue();

                SET_LCD_MODE(GBLCDMODE_OAMRAM);

                mLCDMode           = LCDMODE_LYXX_HBLANK;
                mClocksToNextState = 172 + mScrollXClocks + SPRITE_CLOCKS[mSpriteQueue.size()];// + 16; // The +16 is new.

                break;

            // Mode 1 - Lasts about 204 cycles, depending on how long Mode 3 took
            case LCDMODE_LYXX_HBLANK:
                renderScanline();

                SET_LCD_MODE(GBLCDMODE_HBLANK);

                //H-blank interrupt selected
                if (testBit(mSTAT, 3) && !mLCDCInterrupted)
                {
                    mMemory->requestInterrupt(INTERRUPT_LCD);
                    mLCDCInterrupted = true;
                }

                //HDMA block copy
                if (mHDMAActive)
                {
                    HDMACopyBlock(mHDMASource, mVBK * VRAM_BANK_SIZE + mHDMADestination);
                    mHDMAControl--;
                    mHDMADestination += 0x10;
                    mHDMASource      += 0x10;

                    if ((mHDMAControl & 0x7F) == 0x7F)
                    {
                        mHDMAControl = 0xFF;
                        mHDMAActive  = false;
                    }
                }

                mLCDMode           = LCDMODE_LYXX_HBLANK_INC;
                mClocksToNextState = 200 - mScrollXClocks - SPRITE_CLOCKS[mSpriteQueue.size()];
                break;

            case LCDMODE_LYXX_HBLANK_INC:
                mLY++;

                // Reset LYC bit
                mSTAT = clearBit(mSTAT, 2);

                mLCDCInterrupted   = false;
                mLCDMode           = (mLY == 144) ? LCDMODE_LY9X_VBLANK : LCDMODE_LYXX_OAM;
                mClocksToNextState = 4;
                break;

            // Mode 1 - VBlank period
            case LCDMODE_LY9X_VBLANK:

                // V-blank interrupt
                if (mLY == 144)
                {
                    SET_LCD_MODE(GBLCDMODE_VBLANK);
                    mMemory->requestInterrupt(INTERRUPT_VBLANK);

                    if (testBit(mSTAT, 4))
                        mMemory->requestInterrupt(INTERRUPT_LCD);

                    std::swap(mFrontBuffer, mBackBuffer);
                }

                // Checking LYC=LY in the beginning of scanline
                checkCoincidenceFlag();

                mLCDMode           = LCDMODE_LY9X_VBLANK_INC;
                mClocksToNextState = 452;
                break;

            case LCDMODE_LY9X_VBLANK_INC:
                mLY++;

                // Reset LYC bit
                mSTAT = clearBit(mSTAT, 2);

                // HACK: The emulator does not spend enough time in the V-Blank
                // period right now. It should be spending about 1.1 ms, but it's
                // a bit lower at the moment. By giving it a few extra scanlines
                // we can make up the difference. TODO: Find more proper solution.
                mLCDMode           = (mLY == 157/*153*/) ? LCDMODE_LY00_VBLANK : LCDMODE_LY9X_VBLANK;
                mLCDCInterrupted   = false;
                mClocksToNextState = 4;
                break;

            case LCDMODE_LY00_VBLANK:

                // Checking LYC=LY in the beginning of scanline
                // Here LY = 153
                checkCoincidenceFlag();

                mLY                = 0;
                mLCDMode           = LCDMODE_LY00_HBLANK;
                mClocksToNextState = 452;
                break;

            case LCDMODE_LY00_HBLANK:
                SET_LCD_MODE(GBLCDMODE_HBLANK);

                mLCDCInterrupted   = false;
                mWindowLine        = 0;
                mLCDMode           = LCDMODE_LYXX_OAM;
                mClocksToNextState = 4;
                break;
		}
	}
}

void GBCGraphics::reset()
{
    Graphics::reset();
    
	memset(mVRAM, 0x0, VRAM_BANK_SIZE * 2);
    memset(mBackBuffer, 0xFF, 160 * 144 * 4 * sizeof(mBackBuffer[0]));
    memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));

	// Initially, all background colors are initialized to white
	memset(mBGPD, 0xFF, 64);

	// Initially, all sprite colors are random
	srand(clock());
	for (int i = 0; i < 64; i++)
		mOBPD[i] = rand() % 0x100;

    // Initialize the colors that will be used
    mPalette.reset();
    mPalette.setGamma(mConfig->gbcDisplayGamma);
    mPalette.setSaturation(mConfig->gbcDisplaySaturation);
        
    // LCDC cannot be 0 initially or some games (like Pokemon Red) won't load.	
	mHDMAActive         = false;
	mHDMASource         = 0;
	mHDMADestination    = 0;
	mHDMAControl        = 0;
	mVBK                = 0;
	mBGPI               = 0;
	mOBPI               = 0;
        
    if (mConfig->skipBIOS)
    {
        // Nothing to do
    }
}

byte GBCGraphics::read(const word address) const
{
    if ((address >= VRAM_START) && (address <= VRAM_END))
        return readVRAM(address - VRAM_START);
    else if ((address >= OAM_START) && (address <= OAM_END))
        return readOAM(address - OAM_START);
    
    switch (address)
    {
        case LCDC:  return mLCDC;
        case STAT:  return mSTAT | 0x80;
        case SCY:   return mSCY;
        case SCX:   return mSCX;
        case LY:    return mLY;
        case LYC:   return mLYC;
        case WY:    return mWY;
        case WX:    return mWX;
        case VBK:   return mVBK | 0xFE;
        case HDMA1: return mHDMASource >> 8;
        case HDMA2: return mHDMASource & 0xFF;
        case HDMA3: return mHDMADestination >> 8;
        case HDMA4: return mHDMADestination & 0xFF;
        case HDMA5: return mHDMAControl;
        case BGPI:  return mBGPI | 0x40;
        case BGPD:  return mBGPD[mBGPI & 0x3F];
        case OBPI:  return mOBPI | 0x40;
        case OBPD:  return mOBPD[mOBPI & 0x3F];
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to GBCGraphics." << endl;
            return 0xFF;
        }
    }
}

void GBCGraphics::write(word address, byte data)
{
    if ((address >= VRAM_START) && (address <= VRAM_END))
    {
        writeVRAM(address - VRAM_START, data);
        return;
    }
    else if ((address >= OAM_START) && (address <= OAM_END))
    {
        writeOAM(address - OAM_START, data);
        return;
    }
    
    switch (address)
    {
        case LCDC:  setLCDC(data);       break;
        case STAT:  setSTAT(data);       break;
        case SCY:   mSCY = data;         break;
        case SCX:   mSCX = data;         break;
        case LY:    mLY = 0;             break;
        case LYC:   setLYC(data);        break;
        case DMA:   setDMA(data);        break;
        case WY:    mWY = data;          break;
        case WX:    mWX = data;          break;
        case VBK:   mVBK = (data & 0x1); break;
        case HDMA1: setHDMA1(data);      break;
        case HDMA2: setHDMA2(data);      break;
        case HDMA3: setHDMA3(data);      break;
        case HDMA4: setHDMA4(data);      break;
        case HDMA5: setHDMA5(data);      break;
        case BGPI:  mBGPI = data;        break;
        case BGPD:  setBGPD(data);       break;
        case OBPI:  mOBPI = data;        break;
        case OBPD:  setOBPD(data);       break;
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to GBCGraphics." << endl;
        }
    }
}
    
void GBCGraphics::writeVRAM(word addr, byte value)
{   
    /*if (addr + 0x8000 > 0x9A00 && addr + 0x8000 <= 0x9BFF)
    {
        printf("0x%04x - %d:0x%04x PC: 0x%04x\n", addr + 0x8000, mVBK, value, mMemory->getEmulator()->getCPU()->getProgramCounter());   
        cout.flush();
    }*/
    
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
	{
        mVRAM[mVBK * VRAM_BANK_SIZE + addr] = value;
	}
}

byte GBCGraphics::readVRAM(word addr) const
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
        return mVRAM[mVBK * VRAM_BANK_SIZE + addr];
    
	else return 0xFF;
}

void GBCGraphics::setHDMA1(byte value) 
{ 
	mHDMASource = ((word) value << 8) | (mHDMASource & 0xFF);
}

void GBCGraphics::setHDMA2(byte value) 
{ 
	mHDMASource = (mHDMASource & 0xFF00) | (value & 0xF0);
}

void GBCGraphics::setHDMA3(byte value)
{
	mHDMADestination = (((word)(value & 0x1F)) << 8) | (mHDMADestination & 0xF0);
}

void GBCGraphics::setHDMA4(byte value)
{
	mHDMADestination = (mHDMADestination & 0xFF00) | (value & 0xF0);
}

void GBCGraphics::setHDMA5(byte value)
{
	mHDMAControl = value & 0x7F;

	if (mHDMAActive)
	{
		if (!(value & 0x80))
		{
			mHDMAActive = false;
			mHDMAControl |= 0x80;
		}
	}
	else
	{
        // H-Blank DMA
		if (value & 0x80)
			mHDMAActive = true;
		        
        // General purpose DMA
		else
		{
			mHDMAActive = false;

			word sourceAddr = mHDMASource;
			word destAddr   = mVBK * VRAM_BANK_SIZE + mHDMADestination;
			for (; (mHDMAControl & 0x7F) != 0x7F; mHDMAControl--, destAddr += 0x10, sourceAddr += 0x10)
			{
				if (HDMACopyBlock(sourceAddr, destAddr))
					break;
			}

			mHDMASource      = sourceAddr;
			mHDMADestination = destAddr - (mVBK * VRAM_BANK_SIZE);
			mHDMAControl     = 0xFF;
		}
	}
}

void GBCGraphics::setBGPD(byte value) 
{    
	byte index   = mBGPI & 0x3F;
	mBGPD[index] = value;

	if (testBit(mBGPI, 7))
	{
		index++;
		mBGPI = (mBGPI & 0xC0) | index;
	}
}

void GBCGraphics::setOBPD(byte value) 
{
	byte index   = mOBPI & 0x3F;
	mOBPD[index] = value;

	if (testBit(mOBPI, 7))
	{
		index++;
		mOBPI = (mOBPI & 0xC0) | index;
	}
}

void GBCGraphics::renderScanline()
{
    renderBackground();
    renderWindow();
    renderSprites();
}

void GBCGraphics::renderBackground()
{
    if (mBackgroundGlobalToggle)
    {
        // Work out where tiles and their data come from
        word tileMapAddr  = (testBit(mLCDC, 3) ? 0x9C00 : 0x9800);
        word tileDataAddr = (testBit(mLCDC, 4) ? 0x8000 : 0x8800);

        // Work out which tile we start out in relative to the background map
        word tileX = mSCX / 8;				
        word tileY = ((mSCY + mLY) / 8) & 0x1F;	// Mod 32 to handle wrap around

        // How many pixels into this tile are we? (It's possible to start in the
        // middle of a tile, for example.)
        // & 0x7 == % 8
        byte tileOffsetX = mSCX & 0x7;				    
        byte tileOffsetY = (mSCY + mLY) & 0x7;		    

        int tileIdx;
        for (int x = 0; x < 160; x++)
        {
            // (x, mLY) is the current pixel we're working on
            int screenIndex = mLY * SCREEN_WIDTH_PIXELS + x;
            
            // Figure out where the data for this tile is in memory
            // For GBC, the data is in VRAM bank 0
            word tileAddr = tileMapAddr + (tileY * 32 + tileX);
            if (testBit(mLCDC, 4))
                tileIdx = mVRAM[tileAddr - VRAM_START];
            else
                tileIdx = (signedByte) mVRAM[tileAddr - VRAM_START] + 128;
                        
            // The attributes are always in VRAM bank 1
            byte tileAttributes = mVRAM[VRAM_BANK_SIZE + (tileAddr - VRAM_START)];
            
            byte correctedX = tileOffsetX;
            byte correctedY = tileOffsetY;

            // Apply X flip
            if (testBit(tileAttributes, 5))
                correctedX = 7 - correctedX;

            // Apply Y flip
            if (testBit(tileAttributes, 6))
                correctedY = 7 - correctedY;

            // Figure out where the color for this tile is
            int VBankOffset = VRAM_BANK_SIZE * getBit(tileAttributes, 3);
            int address     = VBankOffset + tileDataAddr + (tileIdx * 16);
            byte colorIdx   = GET_TILE_PIXEL(address - VRAM_START, correctedX, correctedY);
            
            word color;
            memcpy(&color, mBGPD + (tileAttributes & 0x7) * 8 + colorIdx * 2, 2);

            // Store pallete index with BG-to-OAM priority. When the sprites
            // are rendered, they can use this information to determine whether
            // they should be rendered on top of the background or not
            mNativeBuffer[screenIndex] = (tileAttributes & 0x80) | colorIdx;
            screenIndex *= 4;
            
            int c  = mPalette.getColor(color & 0x7FFF);
            mBackBuffer[screenIndex]     = (c >> 16) & 0xFF; // Red
            mBackBuffer[screenIndex + 1] = (c >>  8) & 0xFF; // Green
            mBackBuffer[screenIndex + 2] = (c      ) & 0xFF; // Blue
            mBackBuffer[screenIndex + 3] = 0xFF;             // Alpha

            // Move to the next pixel in this tile (possibly moving to the
            // next tile if we've drawn all the pixels for this tile already)
            tileX       = (tileX + ((tileOffsetX + 1) >> 3)) & 0x1F;
            tileOffsetX = (tileOffsetX + 1) & 0x7;
        }
    }
    else
	{
		//Clearing current scanline as background covers whole screen
        memset(mBackBuffer + (4 * mLY * SCREEN_WIDTH_PIXELS), 0xFF, 160 * 4);
        memset(mNativeBuffer + (mLY * SCREEN_WIDTH_PIXELS), 0x00, 160);
	}
}

void GBCGraphics::renderWindow()
{
    if ((mLCDC & 0x20) && mWindowsGlobalToggle)
	{
        // Checking window visibility on the whole screen and in the scanline
		if (mWX <= 166 && mWY <= 143 && mWindowLine <= 143 && mLY >= mWY)
		{
			word tileMapAddr  = (mLCDC & 0x40) ? 0x1C00 : 0x1800;
			word tileDataAddr = (mLCDC & 0x10) ? 0x0 : 0x800;

			int windowX       = (signed int)mWX - 7;
			word tileMapX     = 0;
			word tileMapY     = (mWindowLine >> 3) & 0x1F;
			byte tileMapTileX = 0;
			byte tileMapTileY = mWindowLine & 0x7;

			// Skipping if window lies outside the screen
			if (windowX < 0)
			{
				int offset   = -windowX;
				tileMapX     = (tileMapX + ((tileMapTileX + offset) >> 3)) & 0x1F;
				tileMapTileX = (tileMapTileX + offset) & 0x7;
				windowX      = 0;
			}

			int tileIdx;
			for (int x = windowX; x < 160; x++)
			{
				word tileIdxAddr = tileMapAddr + tileMapX + tileMapY * 32;
				if (mLCDC & 0x10)
					tileIdx = mVRAM[tileIdxAddr];
				else
					tileIdx = (signed char)mVRAM[tileIdxAddr] + 128;
				
                byte tileAttributes = mVRAM[VRAM_BANK_SIZE + tileIdxAddr];

                byte flippedTileX = tileMapTileX;
                byte flippedTileY = tileMapTileY;

                //Horizontal flip
                if (tileAttributes & 0x20)
                {
                    flippedTileX = 7 - flippedTileX;
                }

                //Vertical flip
                if (tileAttributes & 0x40)
                {
                    flippedTileY = 7 - flippedTileY;
                }

                byte colorIdx = GET_TILE_PIXEL(VRAM_BANK_SIZE * ((tileAttributes >> 3) & 0x1) + tileDataAddr + tileIdx * 16, flippedTileX, flippedTileY);
                word color;
                memcpy(&color, mBGPD + (tileAttributes & 0x7) * 8 + colorIdx * 2, 2);

                //Storing pallet index with BG-to-OAM priority - sprites will need both
                int index = mLY * SCREEN_WIDTH_PIXELS + x;
                mNativeBuffer[index] = (tileAttributes & 0x80) | colorIdx;

                int c  = mPalette.getColor(color & 0x7FFF);
                index *= 4;
                mBackBuffer[index]     = (c >> 16) & 0xFF; // Red
                mBackBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                mBackBuffer[index + 2] = (c      ) & 0xFF; // Blue
                mBackBuffer[index + 3] = 0xFF;             // Alpha
				
				tileMapX     = (tileMapX + ((tileMapTileX + 1) >> 3)) & 0x1F;
				tileMapTileX = (tileMapTileX + 1) & 0x7;
			}

			mWindowLine++;
		}
	}
}

void GBCGraphics::renderSprites()
{
    // Bit 1 determines if sprites should be rendered
    if (testBit(mLCDC, 1) && mSpritesGlobalToggle)
	{
        // Figure out if we're drawing 8x8 sprites or 8x16 sprites
		byte spriteHeight;
		byte tileIdxMask;
		if (testBit(mLCDC, 2))
		{
			spriteHeight = 16;
			tileIdxMask  = 0xFE;
		}
		else
		{
			spriteHeight = 8;
			tileIdxMask  = 0xFF;
		}
        
        // The sprite queue tells us which sprites to render in which order. We
        // go in reverse order because we want sprites with higher priority (those
        // at the front) to be rendered on top of those with lower priority
		for (int i = mSpriteQueue.size() - 1; i >= 0; i--)
		{
            // The lower byte of the sprite queue tells the OAM offset where
            // the sprite can be found. This address, as well as the next
            // 3 define the properties of this sprite
			byte spriteAddr = mSpriteQueue[i] & 0xFF;
            
			// Sprites that are hidden because of their X coordinate affect the
            // sprite queue, but they aren't drawn
			if (mOAM[spriteAddr + 1] == 0 || mOAM[spriteAddr + 1] >= 168)
				continue;

            // Retrieve the sprite properties from OAM.
            //  0 - Sprite y coordinate - 16
            //  1 - Sprite x coordinate - 8
            //  2 - Sprite tile number. In 8x16 mode, the lower bit is ignored
            //  3 - Sprite attributes (priority / x flip / y flip / tile VRAM bank / palette number)
            int spriteY           = mOAM[spriteAddr] - 16;
            int spriteX           = mOAM[spriteAddr + 1] - 8;
            int spriteTileNumber  = mOAM[spriteAddr + 2] & tileIdxMask;
            byte spriteAttributes = mOAM[spriteAddr + 3];
            
            // The lowest 3 bits of the sprite attributes define the palette number
			byte *cgbPalette      = &mOBPD[(spriteAttributes & 0x7) * 8];
            
            // Bit 3 tells which VRAM bank to use. This offset will either be 0 or VRAMBankSize
			word cgbTileMapOffset = VRAM_BANK_SIZE * testBit(spriteAttributes, 3);

			// Determine where this sprite should be placed on the screen
			int spritePixelX = 0;
			int spritePixelY = mLY - spriteY;
            
			if (spriteX < 0)
			{
				spritePixelX = (byte)(spriteX * -1);
				spriteX      = 0;
			}
            
            // Apply X flip
			if (testBit(spriteAttributes, 5))
				spritePixelX = 7 - spritePixelX;
            
            // Apply Y flip
			if (testBit(spriteAttributes, 6))
				spritePixelY = spriteHeight - 1 - spritePixelY;

            // Figure out if we need to go left to right or right to left
            int dx = (testBit(spriteAttributes, 5) ? -1 : 1);
            
			// Determine if sprite is behind the background
			if (testBit(spriteAttributes, 7) && testBit(mLCDC, 0))
			{               
				// Sprites are hidden only behind non-zero colors of the background and window
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
                    int screenIndex = mLY * SCREEN_WIDTH_PIXELS + x;
					colorIdx        = GET_TILE_PIXEL(cgbTileMapOffset + spriteTileNumber * 16, spritePixelX, spritePixelY);

					if ((colorIdx == 0) ||				                                // Sprite color 0 is transparent
                        ((mNativeBuffer[screenIndex] & 0x7) > 0) ||	// Sprite hidden behind colors 1-3
                        (mNativeBuffer[screenIndex] & 0x80))          // Sprite is under the background
                        continue;
                    
                    word color;
                    memcpy(&color, cgbPalette + colorIdx * 2, 2);

                    mNativeBuffer[screenIndex] = colorIdx;
                    screenIndex *= 4;
                    
                    int c  = mPalette.getColor(color & 0x7FFF);
                    mBackBuffer[screenIndex]     = (c >> 16) & 0xFF; // Red
                    mBackBuffer[screenIndex + 1] = (c >>  8) & 0xFF; // Green
                    mBackBuffer[screenIndex + 2] = (c      ) & 0xFF; // Blue
                    mBackBuffer[screenIndex + 3] = 0xFF;             // Alpha
				}
			}
            
            // This sprite should be above the background and the window
			else
			{
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
                    colorIdx = GET_TILE_PIXEL(cgbTileMapOffset + spriteTileNumber * 16, spritePixelX, spritePixelY);

                    // Sprite color 0 is transparent
					if (colorIdx == 0) continue;
						
                    word color;
                    memcpy(&color, cgbPalette + colorIdx * 2, 2);

                    int index = mLY * SCREEN_WIDTH_PIXELS + x;
                    mNativeBuffer[index] = colorIdx;
                    index *= 4;
                    
                    int c  = mPalette.getColor(color & 0x7FFF);
                    mBackBuffer[index]     = (c >> 16) & 0xFF; // Red
                    mBackBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                    mBackBuffer[index + 2] = (c      ) & 0xFF; // Blue
                    mBackBuffer[index + 3] = 0xFF;             // Alpha
				}
			}
		}
	}
}

bool GBCGraphics::HDMACopyBlock(word source, word dest)
{
	if (dest >= (mVBK * VRAM_BANK_SIZE) + VRAM_BANK_SIZE || source == 0xFFFF)
		return true;

	mVRAM[dest + 0x0] = mMemory->read(source + 0x0);
	mVRAM[dest + 0x1] = mMemory->read(source + 0x1);
	mVRAM[dest + 0x2] = mMemory->read(source + 0x2);
	mVRAM[dest + 0x3] = mMemory->read(source + 0x3);
	mVRAM[dest + 0x4] = mMemory->read(source + 0x4);
	mVRAM[dest + 0x5] = mMemory->read(source + 0x5);
	mVRAM[dest + 0x6] = mMemory->read(source + 0x6);
	mVRAM[dest + 0x7] = mMemory->read(source + 0x7);
	mVRAM[dest + 0x8] = mMemory->read(source + 0x8);
	mVRAM[dest + 0x9] = mMemory->read(source + 0x9);
	mVRAM[dest + 0xA] = mMemory->read(source + 0xA);
	mVRAM[dest + 0xB] = mMemory->read(source + 0xB);
	mVRAM[dest + 0xC] = mMemory->read(source + 0xC);
	mVRAM[dest + 0xD] = mMemory->read(source + 0xD);
	mVRAM[dest + 0xE] = mMemory->read(source + 0xE);
	mVRAM[dest + 0xF] = mMemory->read(source + 0xF);

	return false;
}

byte* GBCGraphics::getBackgroundMap(bool printGrid)
{
    const static int BACKGROUND_WIDTH  = 256;
    const static int BACKGROUND_HEIGHT = 256;
    const static int BACKGROUND_TILES  = 32;
    
    // Allocate the buffer that will hold the image
    byte* buffer = new byte[BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 4]();
    
    // Work out where tiles and their data come from
    word tileMapAddr  = (testBit(mLCDC, 3) ? 0x9C00 : 0x9800);
    word tileDataAddr = (testBit(mLCDC, 4) ? 0x8000 : 0x8800);	

    int tileIdx;
    for (int y = 0; y < BACKGROUND_HEIGHT; ++y)
    {
        for (int x = 0; x < BACKGROUND_WIDTH; ++x)
        {
            int tileX       = x / 8;
            int tileY       = y / 8;
            int tileOffsetX = x % 8;
            int tileOffsetY = y % 8;
            
            // Figure out where the data for this tile is in memory
            // For GBC, the data is in VRAM bank 0
            word tileAddr = tileMapAddr + (tileY * BACKGROUND_TILES + tileX);
            if (testBit(mLCDC, 4))
                tileIdx = mVRAM[tileAddr - VRAM_START];
            else
                tileIdx = (signedByte) mVRAM[tileAddr - VRAM_START] + 128;

            // The attributes are always in VRAM bank 1
            byte tileAttributes = mVRAM[VRAM_BANK_SIZE + (tileAddr - VRAM_START)];

            byte correctedX = tileOffsetX;
            byte correctedY = tileOffsetY;

            // Apply X flip
            if (testBit(tileAttributes, 5))
                correctedX = 7 - correctedX;

            // Apply Y flip
            if (testBit(tileAttributes, 6))
                correctedY = 7 - correctedY;

            // Figure out where the color for this tile is
            int VBankOffset = VRAM_BANK_SIZE * getBit(tileAttributes, 3);
            int address     = VBankOffset + tileDataAddr + (tileIdx * 16);
            byte colorIdx   = GET_TILE_PIXEL(address - VRAM_START, correctedX, correctedY);

            word color;
            memcpy(&color, mBGPD + (tileAttributes & 0x7) * 8 + colorIdx * 2, 2);

            int c                   = mPalette.getColor(color & 0x7FFF);
            int screenIndex         = 4 * (y * BACKGROUND_WIDTH + x);
            buffer[screenIndex]     = (c >> 16) & 0xFF; // Red
            buffer[screenIndex + 1] = (c >>  8) & 0xFF; // Green
            buffer[screenIndex + 2] = (c      ) & 0xFF; // Blue
            buffer[screenIndex + 3] = 0xFF;             // Alpha
        }
    }
    
    if (printGrid)
    {
        for (int y = 0; y < BACKGROUND_HEIGHT; ++y)
        {
            for (int x = 0; x < BACKGROUND_WIDTH; ++x)
            {
                if (x % 8 == 0 || y % 8 == 0 || x == BACKGROUND_WIDTH - 1 || y == BACKGROUND_HEIGHT - 1)
                {
                    int screenIndex         = 4 * (y * BACKGROUND_WIDTH + x);
                    buffer[screenIndex]     = 0xFF; // Red
                    buffer[screenIndex + 1] = 0xFF; // Green
                    buffer[screenIndex + 2] = 0xFF; // Blue
                    buffer[screenIndex + 3] = 0xFF; // Alpha
                }
            }
        }
    }
    
    return buffer;
}