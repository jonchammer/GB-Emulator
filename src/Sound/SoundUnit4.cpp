#include "SoundUnit4.h"

SoundUnit4::SoundUnit4(Sound &soundController, EmulatorConfiguration* configuration):
    mSoundController(soundController),
    mConfig(configuration),
    mLengthCounter(0x3F, mStatusBit, 0xFF)
{
	reset();
}

SoundUnit4::~SoundUnit4()
{
}

void SoundUnit4::timerStep(int clockDelta)
{
	mlfsr.step(clockDelta);
}

void SoundUnit4::frameSequencerStep(byte sequencerStep)
{
	mEnvelope.step(sequencerStep);
	mLengthCounter.step(sequencerStep);
}

short SoundUnit4::getWaveLeftOutput()
{
	short leftSwitch    = (mSoundController.read(NR51) >> 7) & 0x1;
	short masterVolume  = (mSoundController.read(NR50) >> 4) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mlfsr.getOutput() * envelopeValue * (masterVolume + 1) * leftSwitch * mStatusBit;
}

short SoundUnit4::getWaveRightOutput()
{
	short rightSwitch   = (mSoundController.read(NR51) >> 3) & 0x1;
	short masterVolume  = mSoundController.read(NR50) & 0x7;
	short envelopeValue = mEnvelope.getEnvelopeValue();

	return mlfsr.getOutput() * envelopeValue * (masterVolume + 1) * rightSwitch * mStatusBit;
}

void SoundUnit4::reset()
{
	NR41Changed(0x0, true);
	NR42Changed(0x0, true);
	NR43Changed(0x0, true);
	NR44Changed(0x0, true);

	mlfsr.reset();
	mEnvelope.reset();
	mLengthCounter.reset();
	mStatusBit = 0;
}

void SoundUnit4::emulateBIOS()
{
    reset();
    NR41Changed(0xFF, true);
}

//Sound length
void SoundUnit4::NR41Changed(byte value, bool override)
{
	if (GBC() && !mSoundController.isSoundEnabled() && !override)
		return;

	//While all sound off only length can be written
	mLengthCounter.NRX1Changed(value);
}

//Envelope function control
void SoundUnit4::NR42Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;

	mEnvelope.NRX2Changed(value);

	if (mEnvelope.disablesSound())
		mStatusBit = 0;
}

//LFSR control
void SoundUnit4::NR43Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;

	mlfsr.NR43Changed(value);
}

//initial, counter/consecutive mode
void SoundUnit4::NR44Changed(byte value, bool override)
{
	if (!mSoundController.isSoundEnabled() && !override)
		return;

	mNR44 = value;

	//If channel initial set
	if (value >> 7)
		mStatusBit = 1;
	
	//In some cases envelope unit disables sound unit
	if (mEnvelope.disablesSound())
		mStatusBit = 0;

	mlfsr.NR44Changed(value);
	mEnvelope.NRX4Changed(value);
	mLengthCounter.NRX4Changed(value);
}

void SoundUnit4::NR52Changed(byte value)
{
	if (!(value >> 7))
	{
		mStatusBit = 0;

		if (GBC())
			NR41Changed(0, true);
		
		NR42Changed(0, true);
		NR43Changed(0, true);
		NR44Changed(0, true);
	}
}