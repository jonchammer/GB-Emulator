#ifndef SOUND_UNIT3_H
#define SOUND_UNIT3_H

#include "../Sound.h"
#include "LengthCounter.h"

class Sound;

/**
 * Outputs voluntary wave patterns from Wave RAM
 *
 * The wave channel plays a 32-entry wave table made up of 4-bit samples. 
 * Each byte encodes two samples, the first in the high bits.
 * The wave channel's frequency timer period is set to (2048-frequency)*2
 */
class SoundUnit3
{
public:
    SoundUnit3(Sound &soundController, EmulatorConfiguration* configuration);
    ~SoundUnit3();

    void timerStep(int clockDelta);
    void frameSequencerStep(byte sequencerStep);

    short getWaveLeftOutput();
    short getWaveRightOutput();

    void reset();
    void emulateBIOS();
    
    byte getStatus()
    {
        return mStatusBit;
    }

    byte getNR30()
    {
        return mNR30 | 0x7F;
    }

    byte getNR31()
    {
        return mLengthCounter.getRNX1();
    }

    byte getNR32()
    {
        return mNR32 | 0x9F;
    }

    byte getNR33()
    {
        return 0xFF;
    }

    byte getNR34()
    {
        return mNR34 | 0xBF;
    }
    byte getWaveRAM(byte pos);

    void NR30Changed(byte value, bool override = false);
    void NR31Changed(byte value, bool override = false);
    void NR32Changed(byte value, bool override = false);
    void NR33Changed(byte value, bool override = false);
    void NR34Changed(byte value, bool override = false);
    void waveRAMChanged(byte pos, byte value);

    void NR52Changed(byte value);

private:

    void calculatePeriod()
    {
        mPeriod = (2048 - (((mNR34 & 0x7) << 8) | mNR33)) * 2;
    }

    Sound& mSoundController;
    EmulatorConfiguration* mConfig;
    
    // Sound 3 I\O registers
    byte mNR30; // Sound on/off (R/W)
    // Only bit 7 can be read
    // Bit 7 - Sound OFF
    //	0: Sound 3 output stop
    //	1: Sound 3 output OK

    byte mNR32; // Select output level (R/W)
    // Only bits 6-5 can be read
    // Bit 6-5 - Select output level
    //	00: Mute
    //	01: Produce Wave Pattern RAM. Data as it is(4 bit length)
    //	10: Produce Wave Pattern RAM. Data shifted once to the RIGHT (1/2) (4 bit length)
    //	11: Produce Wave Pattern RAM. Data shifted twice to the RIGHT (1/4) (4 bit length)

    byte mNR33; // Frequency's lower data (W)
    // Lower 8 bits of an 11 bit frequency

    byte mNR34; // Only bit 6 can be read.
    // Bit 7 - Initial (when set,sound restarts)
    // Bit 6 - Counter/consecutive flag
    // Bit 2-0 - Frequency's higher 3 bits

    byte mWaveRAM[0x10]; // Waveform storage for arbitrary sound data
                         // This storage area holds 32 4-bit samples that 
                         // are played back upper 4 bits first.

    byte mStatusBit;

    //Sample buffer stuff
    byte mSampleBuffer;
    int mSampleIndex;
    int mClockCounter;
    int mPeriod;
    byte mOutput;

    LengthCounter mLengthCounter;
};

#endif