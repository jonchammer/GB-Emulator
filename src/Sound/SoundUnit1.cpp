#include "SoundUnit1.h"

SoundUnit1::SoundUnit1(const bool &_CGB, const bool skipBIOS, Sound &soundController):
    mCGB(_CGB),
    mSoundController(soundController),
    mLengthCounter(0x3F, mStatusBit),
    mSweep(mNR13, mNR14, mStatusBit, mDuty)
{
	reset(skipBIOS);
}

SoundUnit1::~SoundUnit1()
{
}

void SoundUnit1::timerStep(int clockDelta)
{
	mDuty.step(clockDelta);
}

void SoundUnit1::frameSequencerStep(byte sequencerStep)
{
	mSweep.step(sequencerStep);
	mEnvelope.step(sequencerStep);
	mLengthCounter.step(sequencerStep);
}

short SoundUnit1::getWaveLeftOutput()
{
    
	short leftSwitch    = (mSoundController.read(NR51) >> 4) & 0x1;
	short masterVolume  = (mSoundController.read(NR50) >> 4) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mDuty.getOutput() * envelopeValue * (masterVolume + 1) * leftSwitch * mStatusBit;
}

short SoundUnit1::getWaveRightOutput()
{
	short rightSwitch   = mSoundController.read(NR51) & 0x1;
	short masterVolume  = mSoundController.read(NR50) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mDuty.getOutput() * envelopeValue * (masterVolume + 1) * rightSwitch * mStatusBit;
}

void SoundUnit1::reset(const bool skipBIOS)
{
	NR10Changed(0, true);
	NR11Changed(0, true);
	NR12Changed(0, true);
	NR13Changed(0, true);
	NR14Changed(0, true);

	mDuty.reset();
	mSweep.reset();
	mEnvelope.reset();
	mLengthCounter.reset();
	mStatusBit  = 0;
	mWaveOutput = 0;
    
    if (!skipBIOS)
    {
        //On DMG after BIOS executes NR52 contains 0xF1 - status bit for sound 1 is set 
        mStatusBit = 1;
        NR10Changed(0x80, true);
        NR11Changed(0xBF, true);
        NR12Changed(0xF3, true);
        NR14Changed(0xBF, true);
    }
}

//Sweep function control
void SoundUnit1::NR10Changed(byte value, bool override)
{
	if (!mSoundController.getAllSoundEnabled() && !override)
	{
		return;
	}

	mSweep.NRX0Changed(value);
}

//Wave pattern duty, Sound length
void SoundUnit1::NR11Changed(byte value, bool override)
{
	if (mCGB && !mSoundController.getAllSoundEnabled() && !override)
	{
		return;
	}
	if (!mSoundController.getAllSoundEnabled() && !override)
	{
		//While all sound off only length can be written
		value &= mLengthCounter.getLengthMask();
	}
	
	mNR11 = value;

	mDuty.NRX1Changed(value);
	mLengthCounter.NRX1Changed(value);
}

//Envelope function control
void SoundUnit1::NR12Changed(byte value, bool override)
{
	if (!mSoundController.getAllSoundEnabled() && !override)
	{
		return;
	}
	
	mEnvelope.NRX2Changed(value);

	if (mEnvelope.disablesSound())
	{
		mStatusBit = 0;
	}
}

//Low frequency bits
void SoundUnit1::NR13Changed(byte value, bool override)
{
	if (!mSoundController.getAllSoundEnabled() && !override)
	{
		return;
	}

	mNR13 = value;

	mDuty.NRX3Changed(value);
}

//initial, counter/consecutive mode, High frequency bits
void SoundUnit1::NR14Changed(byte value, bool override)
{
	if (!mSoundController.getAllSoundEnabled() && !override)
	{
		return;
	}
	
	mNR14 = value;

	//If channel initial set
	if (value >> 7)
	{
		mStatusBit = 1;
	}
	//In some cases envelope unit disables sound unit
	if (mEnvelope.disablesSound())
	{
		mStatusBit = 0;
	}

	mDuty.NRX4Changed(value);
	mSweep.NRX4Changed(value);
	mEnvelope.NRX4Changed(value);
	mLengthCounter.NRX4Changed(value);
}

void SoundUnit1::NR52Changed(byte value)
{
	if (!(value >> 7))
	{
		mStatusBit = 0;
		
		NR10Changed(0, true);
		if (mCGB)
		{
			NR11Changed(0, true);
		}
		else
		{
			NR11Changed(mNR11 & mLengthCounter.getLengthMask(), true);
		}
		NR12Changed(0, true);
		NR13Changed(0, true);
		NR14Changed(0, true);
	}
}