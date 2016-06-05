#include "SoundUnit3.h"

SoundUnit3::SoundUnit3(Sound &soundController, EmulatorConfiguration* configuration):
    mSoundController(soundController),
    mConfig(configuration),
    mLengthCounter(0xFF, mStatusBit)
{
	reset();
}

SoundUnit3::~SoundUnit3()
{
}

void SoundUnit3::timerStep(int clockDelta)
{
	if (mClockCounter < 0 && (mClockCounter + clockDelta) >= 0)
		mSampleBuffer = mWaveRAM[mSampleIndex >> 1];

	mClockCounter += clockDelta;
	
	if (mClockCounter >= mPeriod)
	{
		int passedPeriods = mClockCounter / mPeriod;
		mClockCounter %= mPeriod;
		
		mSampleIndex  = (mSampleIndex + passedPeriods) & 0x1F;
		mSampleBuffer = mWaveRAM[mSampleIndex >> 1];
		
		mOutput = (mSampleBuffer >> (4 * ((~mSampleIndex) & 0x1))) & 0xF;
		switch ((mNR32 >> 5) & 0x3)
		{
            // Mute
            case 0x0:
                mOutput = 0;
                break;

            // 1:1
            case 0x1:
                break;

            // 1:2
            case 0x2:
                mOutput >>= 1;
                break;

            // 1:4
            case 0x3:
                mOutput >>= 2;
                break;
		}
	}
}

void SoundUnit3::frameSequencerStep(byte sequencerStep)
{
	mLengthCounter.step(sequencerStep);
}

short SoundUnit3::getWaveLeftOutput()
{
	short leftSwitch   = (mSoundController.read(NR51) >> 6) & 0x1;
	short masterVolume = (mSoundController.read(NR50) >> 4) & 0x7;

	return mOutput * (masterVolume + 1) * leftSwitch * mStatusBit;
}

short SoundUnit3::getWaveRightOutput()
{
	short rightSwitch  = (mSoundController.read(NR51) >> 2) & 0x1;
	short masterVolume = mSoundController.read(NR50) & 0x7;

	return mOutput * (masterVolume + 1) * rightSwitch * mStatusBit;
}

void SoundUnit3::reset()
{
	NR30Changed(0x0, true);
	NR31Changed(0x0, true);
	NR32Changed(0x0, true);
	NR33Changed(0x0, true);
	NR34Changed(0x0, true);
    
    memset(mWaveRAM, 0x0, 0x10);

	mLengthCounter.reset();
	mStatusBit    = 0;
	mSampleIndex  = 0;
	mSampleBuffer = 0;
	mClockCounter = 0;
}

void SoundUnit3::emulateBIOS()
{
    reset();
}

//While sound 3 is ON Wave pattern RAM can be read or written only when sound 3 reads samples from it
byte SoundUnit3::getWaveRAM(byte pos)
{ 
	if (mStatusBit)
	{
		if (GBC() || mClockCounter == 1)
			return mWaveRAM[mSampleIndex >> 1];
		
        else return 0xFF;
	}
    
	else return mWaveRAM[pos];
}

void SoundUnit3::waveRAMChanged(byte pos, byte value)
{ 
	if (mStatusBit)
	{
		if (GBC() || mClockCounter == 1)
			mWaveRAM[mSampleIndex >> 1] = value;
	}
    
	else mWaveRAM[pos] = value;
}

//Sound on/off
void SoundUnit3::NR30Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;

	mNR30 = value;

	if (!(value >> 7))
		mStatusBit = 0;
}

//Sound length
void SoundUnit3::NR31Changed(byte value, bool override)
{
	if (GBC() && !mSoundController.isSoundEnabled() && !override)
		return;

	//While all sound off only length can be written
	mLengthCounter.NRX1Changed(value);
}

//Output level
void SoundUnit3::NR32Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;

	mNR32 = value;
}

//Frequency low bits
void SoundUnit3::NR33Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;
	
	mNR33 = value;
	calculatePeriod();
}

//initial, counter/consecutive mode, High frequency bits
void SoundUnit3::NR34Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;
	
	mNR34 = value;
	calculatePeriod();

	//If channel initial set
	if (value & mNR30 & 0x80)
	{
		//Triggering while channel is enabled leads to wave pattern RAM corruption
		if (!GBC() && mClockCounter == 3)
		{
			int waveRAMPos = ((mSampleIndex + 1) & 0x1F) >> 1;
			if (waveRAMPos < 4)
				mWaveRAM[0] = mWaveRAM[waveRAMPos];
            
			else memcpy(mWaveRAM, mWaveRAM + (waveRAMPos & 0xFC), 4); 
		}
		
		//Mostly guesswork
		mClockCounter = -mPeriod - 5;
		mSampleIndex  = 1;

		//If sound 3 On
		mStatusBit = 1;
	}

	mLengthCounter.NRX4Changed(value);
}

void SoundUnit3::NR52Changed(byte value)
{
	if (!(value >> 7))
	{
		mStatusBit = 0;
		
		NR30Changed(0, true);
        
		if (GBC())
			NR31Changed(0, true);

		NR32Changed(0, true);
		NR33Changed(0, true);
		NR34Changed(0, true);
	}
}
