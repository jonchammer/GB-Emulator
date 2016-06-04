#include "Graphics.h"
#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <cstring>

#define LCD_ON() (((mLCDC) & 0x80) != 0)
#define GET_LCD_MODE() ((GameboyLCDModes)(mSTAT & 0x3))
#define SET_LCD_MODE(value) {mSTAT &= 0xFC; mSTAT |= (value);}
#define GET_TILE_PIXEL(VRAMAddr, x, y) ((((mVRAM[(VRAMAddr) + (y) * 2 + 1] >> (7 - (x))) & 0x1) << 1) | ((mVRAM[(VRAMAddr) + (y) * 2] >> (7 - (x))) & 0x1))

Graphics::Graphics(Memory* memory, bool skipBIOS, const bool &_CGB, const bool &_CGBDoubleSpeed, DMGPalettes palette) :
    mMemory(memory),
    mCGB(_CGB),
    mCGBDoubleSpeed(_CGBDoubleSpeed),
    mBackgroundGlobalToggle(true),
    mWindowsGlobalToggle(true),
    mSpritesGlobalToggle(true)
{
    // Attach this component to the memory at the correct locations
    mMemory->attachComponent(this, 0x8000, 0x9FFF); // VRAM
    mMemory->attachComponent(this, 0xFE00, 0xFE9F); // OAM
    mMemory->attachComponent(this, 0xFF40, 0xFF4B); // Graphics I/O ports
    mMemory->attachComponent(this, 0xFF4F, 0xFF4F); // VBK
    mMemory->attachComponent(this, 0xFF51, 0xFF55); // HDMA
    mMemory->attachComponent(this, 0xFF68, 0xFF6B); // GBC Registers
    mMemory->attachComponent(this, 0xFF57, 0xFF67); // ??? - Included by Gomeboy, but don't seem to be useful
    mMemory->attachComponent(this, 0xFF6C, 0xFF6F); // ???
    
    mScreenBuffer = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4];
    mNativeBuffer = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS];
    
	reset(skipBIOS);

	if (palette == RGBPALETTE_REAL)
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

	mSpriteClocks[0]  = 0;
	mSpriteClocks[1]  = 8;
	mSpriteClocks[2]  = 20;
	mSpriteClocks[3]  = 32;
	mSpriteClocks[4]  = 44;
	mSpriteClocks[5]  = 52;
	mSpriteClocks[6]  = 64;
	mSpriteClocks[7]  = 76;
	mSpriteClocks[8]  = 88;
	mSpriteClocks[9]  = 96;
	mSpriteClocks[10] = 108;

	mSpriteQueue = std::vector<int>();
	mSpriteQueue.reserve(40);

	for (int i = 0; i < 32768; i++)
	{
		byte r = (i & 0x1F) << 3;
		byte g = ((i >> 5) & 0x1F) << 3;
		byte b = ((i >> 10) & 0x1F) << 3;
		mGBC2RGBPalette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
}

Graphics::~Graphics()
{
    delete[] mScreenBuffer;
    delete[] mNativeBuffer;
}

void Graphics::update(int clockDelta)
{
	mClockCounter += clockDelta;
	
	if (mOAMDMAStarted)
	{
		DMAStep(clockDelta);
	}

	while (mClockCounter >= mClocksToNextState)
	{
		mClockCounter -= mClocksToNextState;

		if (!LCD_ON())
		{
			mLY = 0;
			mClocksToNextState = 70224;
            memset(mScreenBuffer, 0xFF, 160 * 144 * 4 * sizeof(mScreenBuffer[0]));
            memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));
			mNewFrameReady = true;
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
			checkLYC();
			
			mScrollXClocks     = (mSCX & 0x4) ? 4 : 0;
			mLCDMode           = LCDMODE_LYXX_OAMRAM;
			mClocksToNextState = 80;
			break;

		case LCDMODE_LYXX_OAMRAM:
			prepareSpriteQueue();
			SET_LCD_MODE(GBLCDMODE_OAMRAM);

			mLCDMode           = LCDMODE_LYXX_HBLANK;
			mClocksToNextState = 172 + mScrollXClocks + mSpriteClocks[mSpriteQueue.size()];
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
			if (mCGB && mHDMAActive)
			{
				HDMACopyBlock(mHDMASource, mVRAMBankOffset + mHDMADestination);
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
			mClocksToNextState = 200 - mScrollXClocks - mSpriteClocks[mSpriteQueue.size()];
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
			checkLYC();

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
			checkLYC();
			
			mLY                = 0;
			mLCDMode           = LCDMODE_LY00_HBLANK;
			mClocksToNextState = 452;
			break;

		case LCDMODE_LY00_HBLANK:
			SET_LCD_MODE(GBLCDMODE_HBLANK);

			mLCDCInterrupted = false;

			if (mDelayedWY > -1)
			{
				mWY = mDelayedWY;
				mDelayedWY = -1;
			}

			mNewFrameReady     = true;
			mWindowLine        = 0;
			mLCDMode           = LCDMODE_LYXX_OAM;
			mClocksToNextState = 4;
			break;
		}
	}
}

void Graphics::reset(bool skipBIOS)
{
	memset(mVRAM, 0x0, VRAMBankSize * 2);
	memset(mOAM, 0x0, 0xFF);
    memset(mScreenBuffer, 0xFF, 160 * 144 * 4 * sizeof(mScreenBuffer[0]));
    memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));

	// Initially, all background colors are initialized to white
	memset(mBGPD, 0xFF, 64);

	// Initially, all sprite colors are random
	srand(clock());
	for (int i = 0; i < 64; i++)
		mOBPD[i] = rand() % 0x100;

    // LCDC cannot be 0 initially or some games (like Pokemon Red) won't load.
	mLCDC = 0x00;
	mSTAT = 0x00;
	mSCY  = 0x00;
	mSCX  = 0x00;
	mBGP  = 0x00;
	mOBP0 = 0x00;
	mOBP1 = 0x00;
	mWY   = 0x00;
	mWX   = 0x00;
    mLY   = 0x00;
	mLYC  = 0x00;
    
	mSpriteQueue.clear();
	mNewFrameReady      = false;
	mClockCounter       = 0;
	mClocksToNextState  = 0x0;
	mScrollXClocks      = 0x0;
	mLCDCInterrupted    = false;
	mLCDMode            = LCDMODE_LYXX_OAM;
	mDelayedWY          = -1;
	mWindowLine         = 0;
	mVRAMBankOffset     = 0;
	mHDMAActive         = false;
	mHDMASource         = 0;
	mHDMADestination    = 0;
	mHDMAControl        = 0;
	mVBK                = 0;
	mBGPI               = 0;
	mOBPI               = 0;
	mOAMDMAStarted      = false;
	mOAMDMAClockCounter = 0;
	mOAMDMASource       = 0;
	mOAMDMAProgress     = 0;
        
    if (skipBIOS)
    {
        mLCDC = 0x91;
        mSTAT = 0x85;
        mSCY  = 0x0;
        mSCX  = 0x0;
        mLY   = 0x0;
        mLYC  = 0x0;
        mBGP  = 0xFC;
        mOBP0 = 0xFF;
        mOBP1 = 0xFF;
        mWY   = 0x0;
        mWX   = 0x0;
    }
}

byte Graphics::read(const word address) const
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
            cerr << " does not belong to Graphics." << endl;
            return 0xFF;
        }
    }
}

void Graphics::write(word address, byte data)
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
        case VBK:   VBKChanged(data);   break;
        case HDMA1: HDMA1Changed(data); break;
        case HDMA2: HDMA2Changed(data); break;
        case HDMA3: HDMA3Changed(data); break;
        case HDMA4: HDMA4Changed(data); break;
        case HDMA5: HDMA5Changed(data); break;
        case BGPI:  BGPIChanged(data);  break;
        case BGPD:  BGPDChanged(data);  break;
        case OBPI:  OBPIChanged(data);  break;
        case OBPD:  OBPDChanged(data);  break;
        
        default:    
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to Graphics." << endl;
        }
    }
}
    
void Graphics::writeVRAM(word addr, byte value)
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
	{
		mVRAM[mVRAMBankOffset + addr] = value;
	}
}

void Graphics::writeOAM(byte addr, byte value)
{
	if (GET_LCD_MODE() == GBLCDMODE_HBLANK || GET_LCD_MODE() == GBLCDMODE_VBLANK || !LCD_ON())
		mOAM[addr] = value;
}

byte Graphics::readVRAM(word addr) const
{
	if (GET_LCD_MODE() != GBLCDMODE_OAMRAM || !LCD_ON())
		return mVRAM[mVRAMBankOffset + addr];
    
	else return 0xFF;
}

byte Graphics::readOAM(byte addr) const
{
	if (GET_LCD_MODE() == GBLCDMODE_HBLANK || GET_LCD_MODE() == GBLCDMODE_VBLANK || !LCD_ON())
		return mOAM[addr];
	
	else return 0xFF;
}

void Graphics::DMAStep(int clockDelta)
{
	mOAMDMAClockCounter += clockDelta;

	//In double speed mode OAM DMA runs faster
	int bytesToCopy = 0;
	if (mCGB && mCGBDoubleSpeed)
	{
		bytesToCopy = mOAMDMAClockCounter / 2;
		mOAMDMAClockCounter %= 2;
	}
	else
	{
		bytesToCopy = mOAMDMAClockCounter / 5;
		mOAMDMAClockCounter %= 5;
	}

	if (mOAMDMAProgress + bytesToCopy >= 0xA0)
	{
		mOAMDMAStarted = false;
		bytesToCopy = 0xA0 - mOAMDMAProgress;
	}

	for (int i = 0; i < bytesToCopy; i++, mOAMDMAProgress++)
	{
        mOAM[mOAMDMAProgress] = mMemory->read(mOAMDMASource | mOAMDMAProgress);
	}
}

void Graphics::DMAChanged(byte value)
{
	mOAMDMAStarted      = true;
	mOAMDMASource       = value << 8;
	mOAMDMAClockCounter = 0;
	mOAMDMAProgress     = 0;
}

void Graphics::LCDCChanged(byte value)
{
	// If LCD is being turned off
	if (!(value & 0x80))
	{
		mSTAT &= ~0x3;
		mLY = 0;
	}
    
	// If LCD is being turned on
	else if ((value & 0x80) && !LCD_ON())
	{
		mSpriteQueue.clear();
		mNewFrameReady   = false;
		mClockCounter    = 0;
		mScrollXClocks   = 0;
		mLCDCInterrupted = false;

		mLY = 0;
		checkLYC();

		// When LCD turned on, H-blank is active instead of OAM for 80 cycles
		SET_LCD_MODE(GBLCDMODE_HBLANK);
		mLCDMode = LCDMODE_LYXX_OAMRAM;
		mClocksToNextState = 80;
	}

	if (!(mLCDC & 0x20) && (value & 0x20))
	{
		mWindowLine = 144;
	}

	mLCDC = value;
}

void Graphics::STATChanged(byte value)
{
	// Coincidence flag and mode flag are read-only
	mSTAT = (mSTAT & 0x7) | (value & 0x78); 

	// STAT bug
	if (LCD_ON() && 
		(GET_LCD_MODE() == GBLCDMODE_HBLANK || GET_LCD_MODE() == GBLCDMODE_VBLANK))
	{
        mMemory->requestInterrupt(INTERRUPT_LCD);
	}
}

void Graphics::LYCChanged(byte value) 
{
	if (mLYC != value)
	{
		mLYC = value;
		if (mLCDMode != LCDMODE_LYXX_HBLANK_INC && mLCDMode != LCDMODE_LY9X_VBLANK_INC)
			checkLYC();
	}
    
	else mLYC = value;
}

void Graphics::VBKChanged(byte value)
{ 
	if (mCGB)
	{
		mVBK = value; 
		mVRAMBankOffset = VRAMBankSize * (mVBK & 0x1);
	}
    
	else mVRAMBankOffset = 0;
}

void Graphics::HDMA1Changed(byte value) 
{ 
	if (!mCGB) return;
	mHDMASource = ((word) value << 8) | (mHDMASource & 0xFF);
}

void Graphics::HDMA2Changed(byte value) 
{ 
	if (!mCGB) return;
	mHDMASource = (mHDMASource & 0xFF00) | (value & 0xF0);
}

void Graphics::HDMA3Changed(byte value)
{
	if (!mCGB) return;
	mHDMADestination = (((word)(value & 0x1F)) << 8) | (mHDMADestination & 0xF0);
}

void Graphics::HDMA4Changed(byte value)
{
	if (!mCGB) return;
	mHDMADestination = (mHDMADestination & 0xFF00) | (value & 0xF0);
}

void Graphics::HDMA5Changed(byte value)
{
	if (!mCGB) return;
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
		{
			mHDMAActive = true;
		}
        
        // General purpose DMA
		else
		{
			mHDMAActive = false;

			word sourceAddr = mHDMASource;
			word destAddr   = mVRAMBankOffset + mHDMADestination;
			for (; (mHDMAControl & 0x7F) != 0x7F; mHDMAControl--, destAddr += 0x10, sourceAddr += 0x10)
			{
				if (HDMACopyBlock(sourceAddr, destAddr))
					break;
			}

			mHDMASource      = sourceAddr;
			mHDMADestination = destAddr - mVRAMBankOffset;
			mHDMAControl     = 0xFF;
		}
	}
}

void Graphics::BGPDChanged(byte value) 
{
	if (!mCGB) return;
    
	byte index   = mBGPI & 0x3F;
	mBGPD[index] = value;

	if (mBGPI & 0x80)
	{
		index++;
		mBGPI = (mBGPI & 0xC0) | (index & 0x3F);
	}
}

void Graphics::OBPDChanged(byte value) 
{
	if (!mCGB) return;

	byte index   = mOBPI & 0x3F;
	mOBPD[index] = value;

	if (mOBPI & 0x80)
	{
		index++;
		mOBPI = (mOBPI & 0xC0) | (index & 0x3F);
	}
}

void Graphics::renderScanline()
{
	renderBackground();
    renderWindow();
    renderSprites();
}

void Graphics::renderBackground()
{
    if (((mLCDC & 0x01) || mCGB) && mBackgroundGlobalToggle)
	{
		word tileMapAddr  = (mLCDC & 0x8) ? 0x1C00 : 0x1800;
		word tileDataAddr = (mLCDC & 0x10) ? 0x0 : 0x800;

		word tileMapX     = mSCX >> 3;					// Converting absolute coorditates to tile coordinates i.e. integer division by 8
		word tileMapY     = ((mSCY + mLY) >> 3) & 0x1F;	// then clamping to 0-31 range i.e. wrapping background
		byte tileMapTileX = mSCX & 0x7;				    //
		byte tileMapTileY = (mSCY + mLY) & 0x7;		    //

		int tileIdx;
		for (word x = 0; x < 160; x++)
		{
			word tileAddr = tileMapAddr + tileMapX + tileMapY * 32;
			if (mLCDC & 0x10)
			{
				tileIdx = mVRAM[tileAddr];
			}
			else
			{
				tileIdx = (signed char)mVRAM[tileAddr] + 128;
			}

			if (mCGB)
			{
				byte tileAttributes = mVRAM[VRAMBankSize + tileAddr];

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

                // Storing pallet index with BG-to-OAM priority - sprites will need both
                int index = mLY * SCREEN_WIDTH_PIXELS + x;
				mNativeBuffer[index] = (tileAttributes & 0x80) | colorIdx;
                
                int c  = mGBC2RGBPalette[color & 0x7FFF];
                index *= 4;
                mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                mScreenBuffer[index + 3] = 0xFF;             // Alpha
			}
			else
			{
				byte palIdx = GET_TILE_PIXEL(tileDataAddr + tileIdx * 16, tileMapTileX, tileMapTileY);

                int index = mLY * SCREEN_WIDTH_PIXELS + x;
				mNativeBuffer[index] = (mBGP >> (palIdx * 2)) & 0x3;
                
                int c  = mGB2RGBPalette[mNativeBuffer[index]];
                index *= 4;
                mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                mScreenBuffer[index + 3] = 0xFF;             // Alpha
			}
			 
			tileMapX     = (tileMapX + ((tileMapTileX + 1) >> 3)) & 0x1F;
			tileMapTileX = (tileMapTileX + 1) & 0x7;
		}
	}
	else
	{
		//Clearing current scanline as background covers whole screen
        memset(mScreenBuffer + (4 * mLY * SCREEN_WIDTH_PIXELS), 0xFF, 160 * 4);
        memset(mNativeBuffer + (mLY * SCREEN_WIDTH_PIXELS), 0x00, 160);
	}
}

void Graphics::renderWindow()
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
				{
					tileIdx = mVRAM[tileIdxAddr];
				}
				else
				{
					tileIdx = (signed char)mVRAM[tileIdxAddr] + 128;
				}
				
				if (mCGB)
				{
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
                    mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                    mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                    mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                    mScreenBuffer[index + 3] = 0xFF;             // Alpha
				}
				else
				{
					byte palIdx = GET_TILE_PIXEL(tileDataAddr + tileIdx * 16, tileMapTileX, tileMapTileY);

                    int index = mLY * SCREEN_WIDTH_PIXELS + x;
					mNativeBuffer[index] = (mBGP >> (palIdx * 2)) & 0x3;
                    
					int c = mGB2RGBPalette[mNativeBuffer[index]];
                    index *= 4;
                    mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                    mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                    mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                    mScreenBuffer[index + 3] = 0xFF;             // Alpha
				}
				
				tileMapX     = (tileMapX + ((tileMapTileX + 1) >> 3)) & 0x1F;
				tileMapTileX = (tileMapTileX + 1) & 0x7;
			}

			mWindowLine++;
		}
	}
}

void Graphics::renderSprites()
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

			byte dmgPalette       = dmgPalettes[(mOAM[spriteAddr + 3] >> 4) & 0x1];
			byte *cgbPalette      = &mOBPD[(mOAM[spriteAddr + 3] & 0x7) * 8];
			word cgbTileMapOffset = mVRAMBankOffset * ((mOAM[spriteAddr + 3] >> 3) & 0x1);

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
			if ((mOAM[spriteAddr + 3] & 0x80) && (!mCGB || (mLCDC & 0x1)))
			{
				// sprites are hidden only behind non-zero colors of the background and window
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
					if (mCGB)
					{
						colorIdx = GET_TILE_PIXEL(cgbTileMapOffset + (mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);
					}
					else
					{
						colorIdx = GET_TILE_PIXEL((mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);
					}

					if (colorIdx == 0 ||				    //sprite color 0 is transparent
						(mNativeBuffer[mLY * SCREEN_WIDTH_PIXELS + x] & 0x7) > 0)	//Sprite hidden behind colors 1-3
					{
						continue;
					}
                    
					// If CGB game and priority flag of current background tile is set - background and window on top of sprites
					// Master priority on LCDC can override this but here it's set and 
					if (mCGB && (mNativeBuffer[mLY * SCREEN_WIDTH_PIXELS + x] & 0x80))
					{
						continue;
					}

					if (mCGB)
					{
						word color;
						memcpy(&color, cgbPalette + colorIdx * 2, 2);

                        int index = mLY * SCREEN_WIDTH_PIXELS + x;
						mNativeBuffer[index] = colorIdx;
                        
                        int c = mGBC2RGBPalette[color & 0x7FFF];
                        index *= 4;
                        mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                        mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                        mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                        mScreenBuffer[index + 3] = 0xFF;             // Alpha
					}
					else
					{
                        int index = mLY * SCREEN_WIDTH_PIXELS + x;
						mNativeBuffer[index] = (dmgPalette >> (colorIdx * 2)) & 0x3;
                        
						int c  = mGB2RGBPalette[mNativeBuffer[index]];
                        index *= 4;
                        mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                        mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                        mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                        mScreenBuffer[index + 3] = 0xFF;             // Alpha
					}
				}
			}
			else
			{
				// Sprites are on top of the background and window
				byte colorIdx;
				for (int x = spriteX; x < spriteX + 8 && x < 160; x++, spritePixelX += dx)
				{
					if (mCGB)
					{
						colorIdx = GET_TILE_PIXEL(cgbTileMapOffset + (mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);
					}
					else
					{
						colorIdx = GET_TILE_PIXEL((mOAM[spriteAddr + 2] & tileIdxMask) * 16, spritePixelX, spritePixelY);
					}
					
                    //sprite color 0 is transparent
					if (colorIdx == 0) continue;
						
					if (mCGB)
					{
						word color;
						memcpy(&color, cgbPalette + colorIdx * 2, 2);

                        int index = mLY * SCREEN_WIDTH_PIXELS + x;
						mNativeBuffer[index] = colorIdx;
                        
                        int c  = mGBC2RGBPalette[color & 0x7FFF];
                        index *= 4;
                        mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                        mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                        mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                        mScreenBuffer[index + 3] = 0xFF;             // Alpha
					}
					else
					{
                        int index = mLY * SCREEN_WIDTH_PIXELS + x;
						mNativeBuffer[index] = (dmgPalette >> (colorIdx * 2)) & 0x3;
                        
						int c  = mGB2RGBPalette[mNativeBuffer[index]];
                        index *= 4;
                        mScreenBuffer[index]     = (c >> 16) & 0xFF; // Red
                        mScreenBuffer[index + 1] = (c >>  8) & 0xFF; // Green
                        mScreenBuffer[index + 2] = (c      ) & 0xFF; // Blue
                        mScreenBuffer[index + 3] = 0xFF;             // Alpha
					}
				}
			}
		}
	}
}

void Graphics::checkLYC()
{
	if (mLYC == mLY)
	{
		mSTAT |= 0x4;
        
        // LYC=LY coincidence interrupt selected
		if ((mSTAT & 0x40) && !mLCDCInterrupted)
		{
            mMemory->requestInterrupt(INTERRUPT_LCD);
			mLCDCInterrupted = true;
		}
	}
    
	else mSTAT &= 0xFB;
}

void Graphics::prepareSpriteQueue()
{
	mSpriteQueue.clear();

	if (LCD_ON() && (mLCDC & 0x2))
	{
		int spriteHeight = (mLCDC & 0x4) ? 16 : 8;

		// Building sprite queue
		for (byte i = 0; i < 160; i += 4)
		{
            // Sprite on a current scanline
			if ((mOAM[i] - 16) <= mLY && mLY < (mOAM[i] - 16 + spriteHeight))
			{
                // Building ID that corresponds to the sprite priority
				mSpriteQueue.push_back((mOAM[i + 1] << 8) | i);

				// Max 10 sprites per scanline
				if (mSpriteQueue.size() == 10) break;
			}
		}

		// In CGB mode sprites always ordered according to OAM ordering
		// In DMG mode sprites ordered accroging to X coorinate and OAM ordering (for sprites with equal X coordinate)
		if (!mCGB && mSpriteQueue.size())
			std::sort(mSpriteQueue.begin(), mSpriteQueue.end(), std::less<int>());
	}
}

bool Graphics::HDMACopyBlock(word source, word dest)
{
	if (dest >= mVRAMBankOffset + VRAMBankSize || source == 0xFFFF)
	{
		return true;
	}

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
