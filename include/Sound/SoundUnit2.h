#ifndef SOUND_UNIT2_H
#define SOUND_UNIT2_H

#include "../Sound.h"
#include "DutyUnit.h"
#include "EnvelopeUnit.h"
#include "LengthCounter.h"

class Sound;

/*
Produces quadrangular waves with an envelope. Works exactly line sound unit 1 except there is no frequency sweep unit.

A square channel's frequency timer period is set to (2048-frequency)*4. Four duty cycles are available, 
each waveform taking 8 frequency timer clocks to cycle through.
 */
class SoundUnit2
{
public:
    SoundUnit2(const bool &_CGB, Sound &soundController);
    ~SoundUnit2();

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

    byte getNR21()
    {
        return mLengthCounter.getRNX1();
    }

    byte getNR22()
    {
        return mEnvelope.getNRX2();
    }

    byte getNR23()
    {
        return 0xFF;
    }

    byte getNR24()
    {
        return mNR24 | 0xBF;
    }

    void NR21Changed(byte value, bool override = false);
    void NR22Changed(byte value, bool override = false);
    void NR23Changed(byte value, bool override = false);
    void NR24Changed(byte value, bool override = false);

    void NR52Changed(byte value);

private:

    const bool& mCGB;

    Sound& mSoundController;

    // Sound 2 I\O registers
    byte mNR21; // Sound Length; Wave Pattern Duty (R/W)
    // Only bits 7-6 can be read.
    // Bit 7-6 - Wave pattern duty
    // Bit 5-0 - Sound length data (t1: 0-63)

    byte mNR23; // Frequency lo data (W) 
    // Frequency's lower 8 bits of 11 bit data

    byte mNR24; // Only bit 6 can be read.
    // Bit 7 - Initial (when set, sound restarts)
    // Bit 6 - Counter/consecutive selection
    // Bit 2-0 - Frequency's higher 3 bits

    byte mStatusBit;

    DutyUnit mDuty;
    EnvelopeUnit mEnvelope;
    LengthCounter mLengthCounter;

    int mWaveOutput;
};

#endif