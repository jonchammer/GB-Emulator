#include "Sound.h"
#include <algorithm>

Sound::Sound(const bool &_CGB, const bool skipBIOS, int sampleRate, int sampleBufferLength):
    mSound1GlobalToggle(1),
    mSound2GlobalToggle(1),
    mSound3GlobalToggle(1),
    mSound4GlobalToggle(1),
    mCGB(_CGB),
    mSampleRate(sampleRate),
    mSampleBufferLength(sampleBufferLength),
    mAudioQueue(NULL)
{
	mSampleBuffer = new short[mSampleBufferLength];
	mSamplePeriod = 4194304 / mSampleRate;

	mMasterVolume = 1;

	mSound1 = new SoundUnit1(mCGB, skipBIOS, *this);
	mSound2 = new SoundUnit2(mCGB, skipBIOS, *this);
	mSound3 = new SoundUnit3(mCGB, skipBIOS, *this);
	mSound4 = new SoundUnit4(mCGB, skipBIOS, *this);

	reset(skipBIOS);
}

Sound::~Sound()
{
	delete mSound1;
	delete mSound2;
	delete mSound3;
	delete mSound4;

	delete[] mSampleBuffer;
}

void Sound::update(int clockDelta)
{
	mSound1->timerStep(clockDelta);
	mSound2->timerStep(clockDelta);
	mSound3->timerStep(clockDelta);
	mSound4->timerStep(clockDelta);

	mFrameSequencerClock += clockDelta;
	
	if (mFrameSequencerClock >= 4194304 / 512)
	{
		mFrameSequencerClock -= 4194304 / 512;
		
		mFrameSequencerStep++;
		mFrameSequencerStep &= 0x7;

		if (mAllSoundEnabled)
		{
			mSound1->frameSequencerStep(mFrameSequencerStep);
			mSound2->frameSequencerStep(mFrameSequencerStep);
			mSound3->frameSequencerStep(mFrameSequencerStep);
			mSound4->frameSequencerStep(mFrameSequencerStep);
		}
	}

	mSampleCounter += clockDelta;
	if (mSampleCounter >= mSamplePeriod)
	{
		mSampleCounter %= mSamplePeriod;

		//Mixing is done by adding voltages from every channel
		mSampleBuffer[mSampleBufferPos] = 
            mSound1->getWaveLeftOutput() * mSound1GlobalToggle +
            mSound2->getWaveLeftOutput() * mSound2GlobalToggle + 
            mSound3->getWaveLeftOutput() * mSound3GlobalToggle +
            mSound4->getWaveLeftOutput() * mSound4GlobalToggle;

		mSampleBuffer[mSampleBufferPos + 1] = 
            mSound1->getWaveRightOutput() * mSound1GlobalToggle + 
            mSound2->getWaveRightOutput() * mSound2GlobalToggle +
            mSound3->getWaveRightOutput() * mSound3GlobalToggle + 
            mSound4->getWaveRightOutput() * mSound4GlobalToggle;

		//Amplifying sound
		//Max amplitude for 16-bit audio is 32767. Max channel volume is 15. Max master volume is 7 + 1
		//So gain is 32767 / (15 * 8 * 4) ~ 64
		mSampleBuffer[mSampleBufferPos] *= 64;
		mSampleBuffer[mSampleBufferPos + 1] *= 64;

		//DMG doesn't have this one. This is global volume so we don't need to use OS volume settings to change emulator volume
		mSampleBuffer[mSampleBufferPos]     = short(mSampleBuffer[mSampleBufferPos] * mMasterVolume);
		mSampleBuffer[mSampleBufferPos + 1] = short(mSampleBuffer[mSampleBufferPos + 1] * mMasterVolume);

		mSampleBufferPos += 2;

		//"Resampling" DMG samples to actual sound samples
		if (mSampleBufferPos >= mSampleBufferLength)
		{
			mSampleBufferPos = 0;
			mNewFrameReady = true;
		}
	}
    
    if (mNewFrameReady)
    {
        mNewFrameReady = false;
        if (mAudioQueue != NULL) mAudioQueue->append(mSampleBuffer, mSampleBufferLength);
    }
}

void Sound::write(word address, byte value)
{
    switch (address)
    {
        case NR10: mSound1->NR10Changed(value, false); break;
        case NR11: mSound1->NR11Changed(value, false); break;
        case NR12: mSound1->NR12Changed(value, false); break;
        case NR13: mSound1->NR13Changed(value, false); break;
        case NR14: mSound1->NR14Changed(value, false); break;
        
        case NR21: mSound2->NR21Changed(value, false); break;
        case NR22: mSound2->NR22Changed(value, false); break;
        case NR23: mSound2->NR23Changed(value, false); break;
        case NR24: mSound2->NR24Changed(value, false); break;
        
        case NR30: mSound3->NR30Changed(value, false); break;
        case NR31: mSound3->NR31Changed(value, false); break;
        case NR32: mSound3->NR32Changed(value, false); break;
        case NR33: mSound3->NR33Changed(value, false); break;
        case NR34: mSound3->NR34Changed(value, false); break;
        
        case NR41: mSound4->NR41Changed(value, false); break;
        case NR42: mSound4->NR42Changed(value, false); break;
        case NR43: mSound4->NR43Changed(value, false); break;
        case NR44: mSound4->NR44Changed(value, false); break;
        
        case NR50: NR50Changed(value); break;
        case NR51: NR51Changed(value); break;
        case NR52: NR52Changed(value); break;
        
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
        {
            mSound3->waveRAMChanged((address & 0xFF) - 0x30, value);
            break;
        }
    }
}

byte Sound::read(word address)
{
    switch (address)
    {
        case NR10: return mSound1->getNR10();
        case NR11: return mSound1->getNR11();
        case NR12: return mSound1->getNR12();
        case NR13: return mSound1->getNR13();;
        case NR14: return mSound1->getNR14();
        
        case NR21: return mSound2->getNR21(); 
        case NR22: return mSound2->getNR22(); 
        case NR23: return mSound2->getNR23(); 
        case NR24: return mSound2->getNR24(); 
        
        case NR30: return mSound3->getNR30();
        case NR31: return mSound3->getNR31();
        case NR32: return mSound3->getNR32();
        case NR33: return mSound3->getNR33();
        case NR34: return mSound3->getNR34();
        
        case NR41: return mSound4->getNR41(); 
        case NR42: return mSound4->getNR42(); 
        case NR43: return mSound4->getNR43(); 
        case NR44: return mSound4->getNR44(); 
        
        case NR50: return mNR50; 
        case NR51: return mNR51;
        case NR52: 
        {
            return ((mAllSoundEnabled << 7) | (mSound4->getStatus() << 3) | 
                (mSound3->getStatus() << 2) | (mSound2->getStatus() << 1) | 
                mSound1->getStatus()) | 0x70; 
        }
        
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return mSound3->getWaveRAM((address & 0xFF) - 0x30);
    }
}
       
void Sound::reset(const bool skipBIOS)
{
    mSound1->reset(skipBIOS);
	mSound2->reset(skipBIOS);
	mSound3->reset(skipBIOS);
	mSound4->reset(skipBIOS);

	NR50Changed(0, true);
	NR51Changed(0, true);
	mAllSoundEnabled     = 0;
	mFrameSequencerClock = 0;
	mFrameSequencerStep  = 1;
	mSampleCounter       = 0;
	mSampleBufferPos     = 0;
	mNewFrameReady       = false;
    
    // Initialize variables for BIOS
    if (!skipBIOS)
    {
        NR50Changed(0x77, true);
        NR51Changed(0xF3, true);
        NR52Changed(0xF1);
        mAllSoundEnabled = 1;
    }
}

void Sound::NR50Changed(byte value, bool override)
{
	if (!mAllSoundEnabled && !override)
		return;

	mNR50 = value;
}

void Sound::NR51Changed(byte value, bool override)
{
	if (!mAllSoundEnabled && !override)
		return;

	mNR51 = value;
}

void Sound::NR52Changed(byte value)
{
	//If sound being turned in
	if (value & 0x80)
	{
		if (!mAllSoundEnabled)
		{
			//On power on frame sequencer starts at 1
			mFrameSequencerStep  = 1;
			mFrameSequencerClock = 0;
			
			mSound1->NR52Changed(value);
			mSound2->NR52Changed(value);
			mSound3->NR52Changed(value);
			mSound4->NR52Changed(value);

			//Very important to let know sound units that frame sequencer was reset.
			//Test "08-len ctr during power" requires this to pass
			mSound1->frameSequencerStep(mFrameSequencerStep);
			mSound2->frameSequencerStep(mFrameSequencerStep);
			mSound3->frameSequencerStep(mFrameSequencerStep);
			mSound4->frameSequencerStep(mFrameSequencerStep);
		}
	}
	else
	{
		NR50Changed(0, true);
		NR51Changed(0, true);
		
		mSound1->NR52Changed(value);
		mSound2->NR52Changed(value);
		mSound3->NR52Changed(value);
		mSound4->NR52Changed(value);
	}

	//Only all sound on/off can be changed
	mAllSoundEnabled = value >> 7;
}