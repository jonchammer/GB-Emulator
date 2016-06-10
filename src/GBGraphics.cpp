#include "GBGraphics.h"

GBGraphics::GBGraphics(Memory* memory, EmulatorConfiguration* configuration) :
    Graphics(memory, configuration)
{
    mMemory->attachComponent(this, 0xFF47, 0xFF49); // Gameboy specific ports
    
	if (mConfig->palette == GameboyPalette::REAL)
	{
		//Green palette
		mGB2RGBPalette[0] = 0xFFD1F7E1;
		mGB2RGBPalette[1] = 0xFF72C387;
		mGB2RGBPalette[2] = 0xFF537033;
		mGB2RGBPalette[3] = 0xFF212009;
	}
	else
	{
		mGB2RGBPalette[0] = 0xFFFFFFFF;
		mGB2RGBPalette[1] = 0xFFAAAAAA;
		mGB2RGBPalette[2] = 0xFF555555;
		mGB2RGBPalette[3] = 0xFF000000;
	}
    
    reset();
}

void GBGraphics::update(int clockDelta)
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
			mLY = 0;
			mClocksToNextState = 70224;
            memset(mBackBuffer, 0xFF, 160 * 144 * 4 * sizeof(mBackBuffer[0]));
            memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));
            std::swap(mBackBuffer, mFrontBuffer);
			continue;
		}
		
		switch (mLCDMode)
		{
		case LCDMODE_LYXX_OAM:
			SET_LCD_MODE(GBLCDMODE_OAM);
            
            // OAM interrupt selected
			if ((mSTAT & 0x20) && !mLCDCInterrupted)
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

		case LCDMODE_LYXX_OAMRAM:
			prepareSpriteQueue();
            
            // In DMG mode sprites ordered according to X coordinate and OAM ordering (for sprites with equal X coordinate)
            if (!mSpriteQueue.empty())
                std::sort(mSpriteQueue.begin(), mSpriteQueue.end(), std::less<int>());
            
			SET_LCD_MODE(GBLCDMODE_OAMRAM);

			mLCDMode           = LCDMODE_LYXX_HBLANK;
			mClocksToNextState = 172 + mScrollXClocks + SPRITE_CLOCKS[mSpriteQueue.size()];
			break;

		case LCDMODE_LYXX_HBLANK:
			renderScanline();

			SET_LCD_MODE(GBLCDMODE_HBLANK);
            
            //H-blank interrupt selected
			if ((mSTAT & 0x08) && !mLCDCInterrupted)
			{
                mMemory->requestInterrupt(INTERRUPT_LCD);
				mLCDCInterrupted = true;
			}

			mLCDMode           = LCDMODE_LYXX_HBLANK_INC;
			mClocksToNextState = 200 - mScrollXClocks - SPRITE_CLOCKS[mSpriteQueue.size()];
			break;

		case LCDMODE_LYXX_HBLANK_INC:
			mLY++;
            
			// Reset LYC bit
			mSTAT &= 0xFB;
			
			mLCDCInterrupted   = false;
            mLCDMode           = (mLY == 144) ? LCDMODE_LY9X_VBLANK : LCDMODE_LYXX_OAM;
			mClocksToNextState = 4;
			break;

		// Offscreen LCD modes
		case LCDMODE_LY9X_VBLANK:
            
			// V-blank interrupt
			if (mLY == 144)
			{
				SET_LCD_MODE(GBLCDMODE_VBLANK);
                mMemory->requestInterrupt(INTERRUPT_VBLANK);

				if (mSTAT & 0x10)
                    mMemory->requestInterrupt(INTERRUPT_LCD);
			}

			// Checking LYC=LY in the beginning of scanline
			checkCoincidenceFlag();

			mLCDMode           = LCDMODE_LY9X_VBLANK_INC;
			mClocksToNextState = 452;
			break;

		case LCDMODE_LY9X_VBLANK_INC:
			mLY++;

			// Reset LYC bit
			mSTAT &= 0xFB;

            mLCDMode           = (mLY == 153) ? LCDMODE_LY00_VBLANK : LCDMODE_LY9X_VBLANK;
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

			mLCDCInterrupted = false;
            std::swap(mFrontBuffer, mBackBuffer);
			mWindowLine        = 0;
			mLCDMode           = LCDMODE_LYXX_OAM;
			mClocksToNextState = 4;
			break;
		}
	}
}

void GBGraphics::reset()
{
    Graphics::reset();
    
	memset(mVRAM, 0x0, VRAMBankSize);
    
    // LCDC cannot be 0 initially or some games (like Pokemon Red) won't load.
	mBGP  = 0x00;
	mOBP0 = 0x00;
	mOBP1 = 0x00;
   
    if (mConfig->skipBIOS)
    {   
        mBGP  = 0xFC;
        mOBP0 = 0xFF;
        mOBP1 = 0xFF;
    }
}

byte GBGraphics::read(const word address) const
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
        case BGP:   return mBGP;
        case OBP0:  return mOBP0;
        case OBP1:  return mOBP1;
        case WY:    return mWY;
        case WX:    return mWX;
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to Graphics." << endl;
            return 0xFF;
        }
    }
}

void GBGraphics::write(word address, byte data)
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
        case LCDC:  LCDCChanged(data);  break;
        case STAT:  STATChanged(data);  break;
        case SCY:   mSCY = data;        break;
        case SCX:   mSCX = data;        break;
        case LY:    mLY = 0;            break;
        case LYC:   LYCChanged(data);   break;
        case DMA:   DMAChanged(data);   break;
        case BGP:   mBGP = data;        break;
        case OBP0:  mOBP0 = data;       break;
        case OBP1:  mOBP1 = data;       break;
        case WY:    mWY = data;         break;
        case WX:    mWX = data;         break;
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to Graphics." << endl;
        }
    }
}
    
void GBGraphics::writeVRAM(word addr, byte value)
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
	{
		mVRAM[addr] = value;
	}
}

byte GBGraphics::readVRAM(word addr) const
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
		return mVRAM[addr];
    
	else return 0xFF;
}

void GBGraphics::renderScanline()
{
    renderBackground();
    renderWindow();
    renderSprites();
}

void GBGraphics::renderBackground()
{
    if ((mLCDC & 0x01) && mBackgroundGlobalToggle)
    {
        word tileMapAddr  = (mLCDC & 0x8) ? 0x1C00 : 0x1800;
        word tileDataAddr = (mLCDC & 0x10) ? 0x0 : 0x800;

        word tileMapX     = mSCX >> 3;					// Converting absolute coordinates to tile coordinates i.e. integer division by 8
        word tileMapY     = ((mSCY + mLY) >> 3) & 0x1F;	// then clamping to 0-31 range i.e. wrapping background
        byte tileMapTileX = mSCX & 0x7;				    //
        byte tileMapTileY = (mSCY + mLY) & 0x7;		    //

        int tileIdx;
        for (word x = 0; x < 160; x++)
        {
            word tileAddr = tileMapAddr + tileMapX + tileMapY * 32;
            if (mLCDC & 0x10)
                tileIdx = mVRAM[tileAddr];
            else
                tileIdx = (signed char)mVRAM[tileAddr] + 128;

            byte palIdx = GET_TILE_PIXEL(tileDataAddr + tileIdx * 16, tileMapTileX, tileMapTileY);

            int index = mLY * SCREEN_WIDTH_PIXELS + x;
            mNativeBuffer[index] = (mBGP >> (palIdx * 2)) & 0x3;

            int c  = mGB2RGBPalette[mNativeBuffer[index]];
            index *= 4;
            mBackBuffer[index]     = (c >> 16) & 0xFF; // Red
            mBackBuffer[index + 1] = (c >>  8) & 0xFF; // Green
            mBackBuffer[index + 2] = (c      ) & 0xFF; // Blue
            mBackBuffer[index + 3] = 0xFF;             // Alpha

            tileMapX     = (tileMapX + ((tileMapTileX + 1) >> 3)) & 0x1F;
            tileMapTileX = (tileMapTileX + 1) & 0x7;
        } 
    }
    else
	{
		//Clearing current scanline as background covers whole screen
        memset(mBackBuffer + (4 * mLY * SCREEN_WIDTH_PIXELS), 0xFF, 160 * 4);
        memset(mNativeBuffer + (mLY * SCREEN_WIDTH_PIXELS), 0x00, 160);
	}
}

void GBGraphics::renderWindow()
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

                byte palIdx = GET_TILE_PIXEL(tileDataAddr + tileIdx * 16, tileMapTileX, tileMapTileY);

                int index = mLY * SCREEN_WIDTH_PIXELS + x;
                mNativeBuffer[index] = (mBGP >> (palIdx * 2)) & 0x3;

                int c = mGB2RGBPalette[mNativeBuffer[index]];
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

void GBGraphics::renderSprites()
{
    if ((mLCDC & 0x02) && mSpritesGlobalToggle)
	{
		byte spriteHeight;
		byte tileIdxMask;
		if (mLCDC & 0x04)
		{
			spriteHeight = 16;
			tileIdxMask  = 0xFE;
		}
		else
		{
			spriteHeight = 8;
			tileIdxMask  = 0xFF;
		}

		byte dmgPalettes[2] = {mOBP0, mOBP1};

		for (int i = mSpriteQueue.size() - 1; i >= 0; i--)
		{
			byte spriteAddr = mSpriteQueue[i] & 0xFF;

			//Sprites that are hidden by X coordinate still affect sprite queue
			if (mOAM[spriteAddr + 1] == 0 || mOAM[spriteAddr + 1] >= 168)
				continue;

			byte dmgPalette  = dmgPalettes[(mOAM[spriteAddr + 3] >> 4) & 0x1];
			int spriteX      = mOAM[spriteAddr + 1] - 8;
			int spritePixelX = 0;
			int spritePixelY = mLY - (mOAM[spriteAddr] - 16);
			int dx           = 1;

			if (spriteX < 0)
			{
				spritePixelX = (byte)(spriteX * -1);
				spriteX      = 0;
			}

            // X flip
			if (mOAM[spriteAddr + 3] & 0x20)
			{
				spritePixelX = 7 - spritePixelX;
				dx = -1;
			}
            
            //Y flip
			if (mOAM[spriteAddr + 3] & 0x40)
			{
				spritePixelY = spriteHeight - 1 - spritePixelY;
			}

			// If sprite priority is
			if (mOAM[spriteAddr + 3] & 0x80)
			{
				// sprites are hidden only behind non-zero colors of the background and window
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
					colorIdx = GET_TILE_PIXEL((mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);

					if (colorIdx == 0 ||				    //sprite color 0 is transparent
						(mNativeBuffer[mLY * SCREEN_WIDTH_PIXELS + x] & 0x7) > 0)	//Sprite hidden behind colors 1-3
						continue;
                    
                    int index = mLY * SCREEN_WIDTH_PIXELS + x;
                    mNativeBuffer[index] = (dmgPalette >> (colorIdx * 2)) & 0x3;

                    int c  = mGB2RGBPalette[mNativeBuffer[index]];
                    index *= 4;
                    mBackBuffer[index]     = (c >> 16) & 0xFF; // Red
                    mBackBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                    mBackBuffer[index + 2] = (c      ) & 0xFF; // Blue
                    mBackBuffer[index + 3] = 0xFF;             // Alpha
				}
			}
			else
			{
				// Sprites are on top of the background and window
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
                    colorIdx = GET_TILE_PIXEL((mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);
					
                    //sprite color 0 is transparent
					if (colorIdx == 0) continue;
						
                    int index = mLY * SCREEN_WIDTH_PIXELS + x;
                    mNativeBuffer[index] = (dmgPalette >> (colorIdx * 2)) & 0x3;

                    int c  = mGB2RGBPalette[mNativeBuffer[index]];
                    index *= 4;
                    mBackBuffer[index]     = (c >> 16) & 0xFF; // Red
                    mBackBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                    mBackBuffer[index + 2] = (c      ) & 0xFF; // Blue
                    mBackBuffer[index + 3] = 0xFF;             // Alpha
				}
			}
		}
	}
}