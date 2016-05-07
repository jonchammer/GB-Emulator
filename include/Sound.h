#ifndef SOUND_H
#define SOUND_H

#include "Sound/SoundUnit1.h"
#include "Sound/SoundUnit2.h"
#include "Sound/SoundUnit3.h"
#include "Sound/SoundUnit4.h"
#include "Sound/StreamingAudioQueue.h"

class SoundUnit1;
class SoundUnit2;
class SoundUnit3;
class SoundUnit4;

/*
Passed tests
        01-registers
        02-len ctr
        03-trigger
        04-sweep
        05-sweep details
        06-overflow on trigger
        07-len sweep period sync
        08-len ctr during power
        11-regs after power

        Passed all tests except wave pattern RAM test 
 */

/*
        Frame Sequencer

        Step    Length Ctr  Vol Env     Sweep
        - - - - - - - - - - - - - - - - - - - -
        0       Clock       -           Clock
        1       -           Clock       -
        2       Clock       -           -
        3       -           -           -
        4       Clock       -           Clock
        5       -           -           -
        6       Clock       -           -
        7       -           -           -
        - - - - - - - - - - - - - - - - - - - -
        Rate    256 Hz      64 Hz       128 Hz

        Frame sequencer on reset starts at step 1
 */

// General sound registers
const word NR50 = 0xFF24;
const word NR51 = 0xFF25;
const word NR52 = 0xFF26;

// Sound Unit 1 Registers
const word NR10 = 0xFF10;
const word NR11 = 0xFF11;
const word NR12 = 0xFF12;
const word NR13 = 0xFF13;
const word NR14 = 0xFF14;

// Sound Unit 2 Registers
const word NR21 = 0xFF16;
const word NR22 = 0xFF17;
const word NR23 = 0xFF18;
const word NR24 = 0xFF19;

// Sound Unit 3 Registers
const word NR30 = 0xFF1A;
const word NR31 = 0xFF1B;
const word NR32 = 0xFF1C;
const word NR33 = 0xFF1D;
const word NR34 = 0xFF1E;

// Sound Unit 4 Registers
const word NR41 = 0xFF20;
const word NR42 = 0xFF21;
const word NR43 = 0xFF22;
const word NR44 = 0xFF23;

class Sound
{
public:
    Sound(const bool &_CGB, const bool skipBIOS, int sampleRate = 44100, int sampleBufferLength = 1024);
    ~Sound();

    void update(int clockDelta);
    void write(word address, byte value);
    byte read(word address);
    void reset(const bool skipBIOS);

    bool getAllSoundEnabled()
    {
        return mAllSoundEnabled > 0;
    }

    const short* getSoundFramebuffer()
    {
        return mSampleBuffer;
    }

    bool isNewFrameReady()
    {
        return mNewFrameReady;
    }

    void waitForNewFrame()
    {
        mNewFrameReady = false;
    }

    void setAudioQueue(StreamingAudioQueue* audioQueue)
    {
        mAudioQueue = audioQueue;
    }

    StreamingAudioQueue* getAudioQueue() {return mAudioQueue;}
    
    void setVolume(double vol)
    {
	mMasterVolume = vol;
    }
    
    //Debug

    void toggleSound1()
    {
        mSound1GlobalToggle = ~mSound1GlobalToggle & 0x1;
    }

    void toggleSound2()
    {
        mSound2GlobalToggle = ~mSound2GlobalToggle & 0x1;
    }

    void toggleSound3()
    {
        mSound3GlobalToggle = ~mSound3GlobalToggle & 0x1;
    }

    void toggleSound4()
    {
        mSound4GlobalToggle = ~mSound4GlobalToggle & 0x1;
    }

private:
    void NR50Changed(byte value, bool override = false);
    void NR51Changed(byte value, bool override = false);
    void NR52Changed(byte value);
    
    const bool& mCGB;
    StreamingAudioQueue* mAudioQueue;

    //Sound units
    SoundUnit1* mSound1;
    SoundUnit2* mSound2;
    SoundUnit3* mSound3;
    SoundUnit4* mSound4;

    byte mNR50; // Channel control / ON-OFF / Volume (R/W)
    // Bit 7 - Vin->SO2 ON/OFF
    // Bit 6-4 - SO2 output level (volume) (# 0-7)
    // Bit 3 - Vin->SO1 ON/OFF
    // Bit 2-0 - SO1 output level (volume) (# 0-7)

    byte mNR51; // Selection of Sound output terminal (R/W)
    // Bit 7 - Output sound 4 to SO2 terminal
    // Bit 6 - Output sound 3 to SO2 terminal
    // Bit 5 - Output sound 2 to SO2 terminal
    // Bit 4 - Output sound 1 to SO2 terminal
    // Bit 3 - Output sound 4 to SO1 terminal
    // Bit 2 - Output sound 3 to SO1 terminal
    // Bit 1 - Output sound 2 to SO1 terminal
    // Bit 0 - Output sound 1 to SO1 terminal
    //
    // S02 - left channel
    // S01 - right channel

    byte mAllSoundEnabled;
    // NR52 - Sound on/off (R/W)
    // Bit 7 - All sound on/off
    //	0: stop all sound circuits
    //	1: operate all sound circuits
    // Bit 3 - Sound 4 ON flag
    // Bit 2 - Sound 3 ON flag
    // Bit 1 - Sound 2 ON flag
    // Bit 0 - Sound 1 ON flag
    //
    // We don't use external NR52 register. NR52 value combined from various variables.
    // Status bits located in sound unit classes.

    const int mSampleRate;
    const int mSampleBufferLength;
    int mSamplePeriod;

    int mSampleCounter;
    int mSampleBufferPos;
    short* mSampleBuffer;
    bool mNewFrameReady;

    int mFrameSequencerClock;
    byte mFrameSequencerStep;

    double mMasterVolume;

    //Debug
    int mSound1GlobalToggle;
    int mSound2GlobalToggle;
    int mSound3GlobalToggle;
    int mSound4GlobalToggle;
};

#endif