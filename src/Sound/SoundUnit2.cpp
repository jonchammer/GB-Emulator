#include "SoundUnit2.h"

SoundUnit2::SoundUnit2(const bool &_CGB, const bool skipBIOS, Sound &soundController):
    mCGB(_CGB),
    mSoundController(soundController),
    mLengthCounter(0x3F, mStatusBit)
{
	reset(skipBIOS);
}

SoundUnit2::~SoundUnit2()
{
}

void SoundUnit2::timerStep(int clockDelta)
{
	mDuty.step(clockDelta);
}

void SoundUnit2::frameSequencerStep(byte sequencerStep)
{
	mEnvelope.step(sequencerStep);
	mLengthCounter.step(sequencerStep);
}

short SoundUnit2::getWaveLeftOutput()
{
	short leftSwitch    = (mSoundController.read(NR51) >> 5) & 0x1;
	short masterVolume  = (mSoundController.read(NR50) >> 4) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mDuty.getOutput() * envelopeValue * (masterVolume + 1) * leftSwitch * mStatusBit;
}

short SoundUnit2::getWaveRightOutput()
{
	short rightSwitch   = (mSoundController.read(NR51) >> 1) & 0x1;
	short masterVolume  = mSoundController.read(NR50) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mDuty.getOutput() * envelopeValue * (masterVolume + 1) * rightSwitch * mStatusBit;
}

void SoundUnit2::reset(const bool skipBIOS)
{
	NR21Changed(0, true);
	NR22Changed(0, true);
	NR23Changed(0, true);
	NR24Changed(0, true);

	mStatusBit = 0;
    if (!skipBIOS)
    {
//        NR21Changed(0x3F, true);
//        NR22Changed(0x00, true);
//        NR24Changed(0xBF, true);
        NR21Changed(0x80, true);
        NR22Changed(0xF3, true);
    }
}

//Wave pattern duty, Sound length
void SoundUnit2::NR21Changed(byte value, bool override)
{
	if (mCGB && !mSoundController.isSoundEnabled() && !override)
	{
		return;
	}
	if (!mSoundController.isSoundEnabled() && !override)
	{
		//While all sound off only length can be written
		value &= mLengthCounter.getLengthMask();
	}

	mNR21 = value;

	mDuty.NRX1Changed(value);
	mLengthCounter.NRX1Changed(value);
}

//Envelope function control
void SoundUnit2::NR22Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
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
void SoundUnit2::NR23Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}

	mNR23 = value;

	mDuty.NRX3Changed(value);
}

//initial, counter/consecutive mode, High frequency bits
void SoundUnit2::NR24Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
	{
		return;
	}

	mNR24 = value;

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
	mEnvelope.NRX4Changed(value);
	mLengthCounter.NRX4Changed(value);
}

void SoundUnit2::NR52Changed(byte value)
{
	if (!(value >> 7))
	{
		mStatusBit = 0;

		if (mCGB)
		{
			NR21Changed(0, true);
		}
		else
		{
			NR21Changed(mNR21 & mLengthCounter.getLengthMask(), true);
		}
		NR22Changed(0, true);
		NR23Changed(0, true);
		NR24Changed(0, true);
	}
}