#include "SoundUnit3.h"

SoundUnit3::SoundUnit3(const bool &_CGB, const bool skipBIOS, Sound &soundController):
    mCGB(_CGB),
    mSoundController(soundController),
    mLengthCounter(0xFF, mStatusBit)
{
	reset(skipBIOS);
}

SoundUnit3::~SoundUnit3()
{
}

void SoundUnit3::timerStep(int clockDelta)
{
	if (mClockCounter < 0 && (mClockCounter + clockDelta) >= 0)
	{
		mSampleBuffer = mWaveRAM[mSampleIndex >> 1];
	}

	mClockCounter += clockDelta;
	
	if (mClockCounter >= mPeriod)
	{
		int passedPeriods = mClockCounter / mPeriod;
		mClockCounter %= mPeriod;
		
		mSampleIndex = (mSampleIndex + passedPeriods) & 0x1F;
		mSampleBuffer = mWaveRAM[mSampleIndex >> 1];
		
		mOutput = (mSampleBuffer >> (4 * ((~mSampleIndex) & 0x1))) & 0xF;
		switch ((mNR32 >> 5) & 0x3)
		{
		case 0x0://mute
			mOutput = 0;
			break;

		case 0x1://1:1
			break;

		case 0x2://1:2
			mOutput >>= 1;
			break;

		case 0x3://1:4
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

void SoundUnit3::reset(const bool skipBIOS)
{
	NR30Changed(0, true);
	NR31Changed(0, true);
	NR32Changed(0, true);
	NR33Changed(0, true);
	NR34Changed(0, true);
    for (int i = 0; i < 0x10; ++i)
        mWaveRAM[i] = 0;

	mLengthCounter.reset();
	mStatusBit    = 0;
	mSampleIndex  = 0;
	mSampleBuffer = 0;
	mClockCounter = 0;
    
    // Sound Unit 3 isn't used during the BIOS, so we don't
    // have to do anything special for it.
//    if (!skipBIOS)
//    {
//        NR30Changed(0x7F, true);
//        NR31Changed(0xFF, true);
//        NR32Changed(0x9F, true);
//        NR34Changed(0xBF, true);
//    }
}

//While sound 3 is ON Wave pattern RAM can be read or written only when sound 3 reads samples from it
byte SoundUnit3::getWaveRAM(byte pos)
{ 
	if (mStatusBit)
	{
		if (mCGB || mClockCounter == 1)
		{
			return mWaveRAM[mSampleIndex >> 1];
		}
		else
		{
			return 0xFF;
		}
	}
	else
	{
		return mWaveRAM[pos];
	}
}

void SoundUnit3::waveRAMChanged(byte pos, byte value)
{ 
	if (mStatusBit)
	{
		if (mCGB || mClockCounter == 1)
		{
			mWaveRAM[mSampleIndex >> 1] = value;
		}
	}
	else
	{
		mWaveRAM[pos] = value;
	}
}

//Sound on/off
void SoundUnit3::NR30Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}

	mNR30 = value;

	if (!(value >> 7))
	{
		mStatusBit = 0;
	}
}

//Sound length
void SoundUnit3::NR31Changed(byte value, bool override)
{
	if (mCGB && !mSoundController.isSoundEnabled() && !override)
	{
		return;
	}

	//While all sound off only length can be written
	mLengthCounter.NRX1Changed(value);
}

//Output level
void SoundUnit3::NR32Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}

	mNR32 = value;
}

//Frequency low bits
void SoundUnit3::NR33Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}
	
	mNR33 = value;

	calculatePeriod();
}

//initial, counter/consecutive mode, High frequency bits
void SoundUnit3::NR34Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}
	
	mNR34 = value;

	calculatePeriod();

	//If channel initial set
	if (value & mNR30 & 0x80)
	{
		//Triggering while channel is enabled leads to wave pattern RAM corruption
		if (!mCGB && mClockCounter == 3)
		{
			int waveRAMPos = ((mSampleIndex + 1) & 0x1F) >> 1;
			if (waveRAMPos < 4)
			{
				mWaveRAM[0] = mWaveRAM[waveRAMPos];
			}
			else
			{
				memcpy(mWaveRAM, mWaveRAM + (waveRAMPos & 0xFC), 4); 
			}
		}
		
		//Mostly guesswork
		mClockCounter = -mPeriod - 5;
		mSampleIndex = 1;

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
		if (mCGB)
		{
			NR31Changed(0, true);
		}
		NR32Changed(0, true);
		NR33Changed(0, true);
		NR34Changed(0, true);
	}
}
