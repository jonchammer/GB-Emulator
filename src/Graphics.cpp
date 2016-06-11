#include "Graphics.h"

Graphics::Graphics(Memory* memory, EmulatorConfiguration* configuration) : 
    mMemory(memory), mConfig(configuration),
    mBackgroundGlobalToggle(true), mWindowsGlobalToggle(true), mSpritesGlobalToggle(true)
{
    // Attach this component to the memory at the correct locations
    mMemory->attachComponent(this, 0x8000, 0x9FFF); // VRAM
    mMemory->attachComponent(this, 0xFE00, 0xFE9F); // OAM
    mMemory->attachComponent(this, 0xFF40, 0xFF46); // GBGraphics I/O ports
    mMemory->attachComponent(this, 0xFF4A, 0xFF4B); // WX, WY
    
	mSpriteQueue.reserve(40); 
    
    mFrontBuffer  = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4];
    mBackBuffer   = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS * 4];
    mNativeBuffer = new byte[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS];
    
    reset();
}

Graphics::~Graphics()
{
    delete[] mFrontBuffer;
    delete[] mBackBuffer;
    delete[] mNativeBuffer;
    
    mFrontBuffer  = NULL;
    mBackBuffer   = NULL;
    mNativeBuffer = NULL;
}

void Graphics::reset()
{
    // Reset the storage
    mSpriteQueue.clear();
    memset(mOAM, 0x0, 0xA0);
    memset(mBackBuffer, 0xFF, 160 * 144 * 4 * sizeof(mBackBuffer[0]));
    memset(mNativeBuffer, 0x00, 160 * 144 * sizeof(mNativeBuffer[0]));
    
    // Reset the registers
    mLCDC = 0x00;
    mSTAT = 0x00;
    mLY   = 0x00;  
    mLYC  = 0x00;
    mSCY  = 0x00;
	mSCX  = 0x00;
    mWY   = 0x00;
	mWX   = 0x00;
    
    // Reset the misc. variables needed by other methods
    mClockCounter       = 0;
	mClocksToNextState  = 0x0;
	mScrollXClocks      = 0x0;
	mLCDCInterrupted    = false;
	mLCDMode            = LCDMODE_LYXX_OAM;
	mWindowLine         = 0;
    mOAMDMAStarted      = false;
	mOAMDMAClockCounter = 0;
	mOAMDMASource       = 0;
	mOAMDMAProgress     = 0;
    
    // Handle differences when the BIOS is skipped
    if (mConfig->skipBIOS)
    {
        mLCDC = 0x91;
        mSTAT = 0x85;
    }
}

void Graphics::prepareSpriteQueue()
{
	mSpriteQueue.clear();

    // Bit 1 determines if sprites are rendered
	if (LCD_ON() && testBit(mLCDC, 1))
	{
        // Work out the size of the sprites
		int spriteHeight = (testBit(mLCDC, 2) ? 16 : 8);

        const int OAM_SIZE = 160;
        
		// Building sprite queue
		for (byte i = 0; i < OAM_SIZE; i += 4)
		{
            // First value in OAM is y position - 16.
            // second is x position - 8. Since all sprites have an implied -8,
            // we don't need to deal with it explicitly though
            byte spriteY = mOAM[i] - 16;
            byte spriteX = mOAM[i + 1];
            
            // Is this sprite supposed to be drawn on this scanline?
			if (mLY >= spriteY && mLY < (spriteY + spriteHeight))
			{
                // Build an ID that corresponds to the sprite priority
				mSpriteQueue.push_back(spriteX << 8 | i);

				// Max 10 sprites per scanline
				if (mSpriteQueue.size() == 10) break;
			}
		}
	}
}

void Graphics::checkCoincidenceFlag()
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

void Graphics::writeOAM(byte addr, byte value)
{
	if (GET_LCD_MODE() == GBLCDMODE_HBLANK || GET_LCD_MODE() == GBLCDMODE_VBLANK || !LCD_ON())
		mOAM[addr] = value;
}

byte Graphics::readOAM(byte addr) const
{
	if (GET_LCD_MODE() == GBLCDMODE_HBLANK || GET_LCD_MODE() == GBLCDMODE_VBLANK || !LCD_ON())
		return mOAM[addr];
	
	else return 0xFF;
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
		mClockCounter    = 0;
		mScrollXClocks   = 0;
		mLCDCInterrupted = false;

		mLY = 0;
		checkCoincidenceFlag();

		// When LCD turned on, H-blank is active instead of OAM for 80 cycles
		SET_LCD_MODE(GBLCDMODE_HBLANK);
		mLCDMode = LCDMODE_LYXX_OAMRAM;
		mClocksToNextState = 80;
	}

	if (!(mLCDC & 0x20) && (value & 0x20))
		mWindowLine = 144;

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
			checkCoincidenceFlag();
	}
    
	else mLYC = value;
}

void Graphics::DMAStep(int clockDelta)
{
	mOAMDMAClockCounter += clockDelta;

	//In double speed mode OAM DMA runs faster
	int bytesToCopy = 0;
	if (GBC(mConfig) && mConfig->doubleSpeed)
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
        mOAM[mOAMDMAProgress] = mMemory->read(mOAMDMASource | mOAMDMAProgress);
}

void Graphics::DMAChanged(byte value)
{
	mOAMDMAStarted      = true;
	mOAMDMASource       = value << 8;
	mOAMDMAClockCounter = 0;
	mOAMDMAProgress     = 0;
}