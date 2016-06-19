#include "GBCGraphics.h"
#include <cmath>

void RGBToHSL(byte red, byte green, byte blue, double& h, double& s, double& l)
{
    double r = red   / 255.0;
    double g = green / 255.0;
    double b = blue  / 255.0;
    
    double r2, g2, b2;

    h = 0.0, s = 0.0, l = 0.0; // default to black
    double maxRGB = max(max(r, g), b);
    double minRGB = min(min(r, g), b);

    l = (minRGB + maxRGB) / 2.0;
    if (l <= 0.0) return;

    double vm = maxRGB - minRGB;
    s = vm;
    if (s <= 0.0) return;
    
    s /= ((l <= 0.5) ? (maxRGB + minRGB ) : (2.0 - maxRGB - minRGB));

    r2 = (maxRGB - r) / vm;
    g2 = (maxRGB - g) / vm;
    b2 = (maxRGB - b) / vm;
    
    if (r == maxRGB)
          h = (g == minRGB ? 5.0 + b2 : 1.0 - g2);
    else if (g == maxRGB)
          h = (b == minRGB ? 1.0 + r2 : 3.0 - b2);
    else
          h = (r == minRGB ? 3.0 + g2 : 5.0 - r2);
 
    h /= 6.0;
}

void HSLToRGB(double h, double s, double l, byte& red, byte& green, byte& blue)
{
    double r = l, g = l, b = l;   // default to gray
    double v = ((l <= 0.5) ? (l * (1.0 + s)) : (l + s - l * s));
    
    if (v > 0)
    {
        double m  = l + l - v;
        double sv = (v - m ) / v;
        
        h *= 6.0;
        
        int sextant  = (int)h;
        double fract = h - sextant;
        double vsf   = v * sv * fract;
        double mid1  = m + vsf;
        double mid2  = v - vsf;
        
        switch (sextant)
        {
            case 0:
                r = v;
                g = mid1;
                b = m;
                break;
            case 1:
                r = mid2;
                g = v;
                b = m;
                break;
            case 2:
                r = m;
                g = v;
                b = mid1;
                break;
            case 3:
                r = m;
                g = mid2;
                b = v;
                break;
            case 4:
                r = mid1;
                g = m;
                b = v;
                break;
            case 5:
                r = v;
                g = m;
                b = mid2;
                break;
          }
    }
    
    // Convert from [0, 1] to [0, 255]
    red   = r * 255;
    green = g * 255;
    blue  = b * 255;
}

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

			//HDMA block copy
			if (mHDMAActive)
			{
				HDMACopyBlock(mHDMASource, mVBK * VRAMBankSize + mHDMADestination);
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

void GBCGraphics::reset()
{
    Graphics::reset();
    
	memset(mVRAM, 0x0, VRAMBankSize * 2);
    memset(mBackBuffer, 0xFF, 160 * 144 * 4 * sizeof(mBackBuffer[0]));
    memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));

	// Initially, all background colors are initialized to white
	memset(mBGPD, 0xFF, 64);

	// Initially, all sprite colors are random
	srand(clock());
	for (int i = 0; i < 64; i++)
		mOBPD[i] = rand() % 0x100;

    // Initialize the colors that will be used
    double h, s, l;
    double gamma = 1.0 / mConfig->gbcDisplayGamma;
    
    for (int i = 0; i < 32768; i++)
	{
		byte b = ( i        & 0x1F) << 3;
		byte g = ((i >>  5) & 0x1F) << 3;
		byte r = ((i >> 10) & 0x1F) << 3;
                
        // Convert the color to HSL so we can manipulate it easier
        RGBToHSL(r, g, b, h, s, l);
        
        // Apply corrections to make the color look more like a real GBC
        // (as long as the configuration says to do so)
        s *= mConfig->gbcDisplayColor;
        l = pow(l, gamma);
        
        // Convert the color back to RGB
        HSLToRGB(h, s, l, r, g, b);
        
		mGBC2RGBPalette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
    
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
        case LCDC:  LCDCChanged(data);  break;
        case STAT:  STATChanged(data);  break;
        case SCY:   mSCY = data;        break;
        case SCX:   mSCX = data;        break;
        case LY:    mLY = 0;            break;
        case LYC:   LYCChanged(data);   break;
        case DMA:   DMAChanged(data);   break;
        case WY:    mWY = data;         break;
        case WX:    mWX = data;         break;
        case VBK:   mVBK = (data & 0x1);break;
        case HDMA1: HDMA1Changed(data); break;
        case HDMA2: HDMA2Changed(data); break;
        case HDMA3: HDMA3Changed(data); break;
        case HDMA4: HDMA4Changed(data); break;
        case HDMA5: HDMA5Changed(data); break;
        case BGPI:  mBGPI = data;       break;
        case BGPD:  BGPDChanged(data);  break;
        case OBPI:  mOBPI = data;       break;
        case OBPD:  OBPDChanged(data);  break;
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to GBCGraphics." << endl;
        }
    }
}
    
void GBCGraphics::writeVRAM(word addr, byte value)
{   
    if (addr == (0x9B00-0x8000)/* && value == 0x54)*/)
    {
        printf("%d:0x%04x\n", mVBK, value);
        cout.flush();
    }
    
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
	{
        mVRAM[mVBK * VRAMBankSize + addr] = value;
	}
}

byte GBCGraphics::readVRAM(word addr) const
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
        return mVRAM[mVBK * VRAMBankSize + addr];
    
	else return 0xFF;
}

void GBCGraphics::HDMA1Changed(byte value) 
{ 
	mHDMASource = ((word) value << 8) | (mHDMASource & 0xFF);
}

void GBCGraphics::HDMA2Changed(byte value) 
{ 
	mHDMASource = (mHDMASource & 0xFF00) | (value & 0xF0);
}

void GBCGraphics::HDMA3Changed(byte value)
{
	mHDMADestination = (((word)(value & 0x1F)) << 8) | (mHDMADestination & 0xF0);
}

void GBCGraphics::HDMA4Changed(byte value)
{
	mHDMADestination = (mHDMADestination & 0xFF00) | (value & 0xF0);
}

void GBCGraphics::HDMA5Changed(byte value)
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
			word destAddr   = mVBK * VRAMBankSize + mHDMADestination;
			for (; (mHDMAControl & 0x7F) != 0x7F; mHDMAControl--, destAddr += 0x10, sourceAddr += 0x10)
			{
				if (HDMACopyBlock(sourceAddr, destAddr))
					break;
			}

			mHDMASource      = sourceAddr;
			mHDMADestination = destAddr - (mVBK * VRAMBankSize);
			mHDMAControl     = 0xFF;
		}
	}
}

void GBCGraphics::BGPDChanged(byte value) 
{    
	byte index   = mBGPI & 0x3F;
	mBGPD[index] = value;

	if (testBit(mBGPI, 7))
	{
		index++;
		mBGPI = (mBGPI & 0xC0) | index;
	}
}

void GBCGraphics::OBPDChanged(byte value) 
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
            byte tileAttributes = mVRAM[VRAMBankSize + (tileAddr - VRAM_START)];
            
            // TEMPORARY: Zelda DX HACK
//            {
//                if (tileX == 0 && tileY == 24 && tileAddr == 0x9B00)
//                {
//                    tileIdx = 0x54 + 128;
//                    tileAttributes = 0x01;
//                }
//                if (tileX == 1 && tileY == 24 && tileAddr == 0x9B01)
//                {
//                    tileIdx = 0x55 + 128;
//                    tileAttributes = 0x01;
//                }
//            }
            
            byte correctedX = tileOffsetX;
            byte correctedY = tileOffsetY;

            // Apply X flip
            if (testBit(tileAttributes, 5))
                correctedX = 7 - correctedX;

            // Apply Y flip
            if (testBit(tileAttributes, 6))
                correctedY = 7 - correctedY;

            // Figure out where the color for this tile is
            int VBankOffset = VRAMBankSize * getBit(tileAttributes, 3);
            int address     = VBankOffset + tileDataAddr + (tileIdx * 16);
            byte colorIdx   = GET_TILE_PIXEL(address - VRAM_START, correctedX, correctedY);
            
            word color;
            memcpy(&color, mBGPD + (tileAttributes & 0x7) * 8 + colorIdx * 2, 2);

            // Store pallete index with BG-to-OAM priority. When the sprites
            // are rendered, they can use this information to determine whether
            // they should be rendered on top of the background or not
            mNativeBuffer[screenIndex] = (tileAttributes & 0x80) | colorIdx;
            screenIndex *= 4;
            
            int c  = mGBC2RGBPalette[color & 0x7FFF];
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
				
                byte tileAttributes = mVRAM[VRAMBankSize + tileIdxAddr];

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

                byte colorIdx = GET_TILE_PIXEL(VRAMBankSize * ((tileAttributes >> 3) & 0x1) + tileDataAddr + tileIdx * 16, flippedTileX, flippedTileY);
                word color;
                memcpy(&color, mBGPD + (tileAttributes & 0x7) * 8 + colorIdx * 2, 2);

                //Storing pallet index with BG-to-OAM priority - sprites will need both
                int index = mLY * SCREEN_WIDTH_PIXELS + x;
                mNativeBuffer[index] = (tileAttributes & 0x80) | colorIdx;

                int c = mGBC2RGBPalette[color & 0x7FFF];
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
			word cgbTileMapOffset = VRAMBankSize * testBit(spriteAttributes, 3);

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
                    
                    int c = mGBC2RGBPalette[color & 0x7FFF];
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
                    
                    int c  = mGBC2RGBPalette[color & 0x7FFF];
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
	if (dest >= (mVBK * VRAMBankSize) + VRAMBankSize || source == 0xFFFF)
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
