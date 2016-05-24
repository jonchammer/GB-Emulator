#ifndef SOUND_UNIT1_H
#define SOUND_UNIT1_H

#include "../Sound.h"
#include "DutyUnit.h"
#include "SweepUnit.h"
#include "EnvelopeUnit.h"
#include "LengthCounter.h"

class Sound;

/**
 * Produces quadrangular waves with sweep and envelope functions.
 *
 * A square channel's frequency timer period is set to (2048-frequency)*4. 
 * Four duty cycles are available, each waveform taking 8 frequency timer clocks 
 * to cycle through.
 */
class SoundUnit1
{
public:
    SoundUnit1(const bool &_CGB, Sound &soundController);
    ~SoundUnit1();

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

    byte getNR10()
    {
        return mSweep.getNRX0();
    }

    byte getNR11()
    {
        return mNR11 | 0x3F;
    }

    byte getNR12()
    {
        return mEnvelope.getNRX2();
    }

    byte getNR13()
    {
        return 0xFF;
    }

    byte getNR14()
    {
        return mNR14 | 0xBF;
    }

    void NR10Changed(byte value, bool override = false);
    void NR11Changed(byte value, bool override = false);
    void NR12Changed(byte value, bool override = false);
    void NR13Changed(byte value, bool override = false);
    void NR14Changed(byte value, bool override = false);

    void NR52Changed(byte value);

private:

    const bool& mCGB;

    Sound& mSoundController;

    // Sound 1 I\O registers
    byte mNR11; // Sound length/Wave pattern duty (R/W)
    // Only Bits 7-6 can be read.
    // Bit 7-6 - Wave Pattern Duty
    // Bit 5-0 - Sound length data (t1: 0-63)

    byte mNR13; // Frequency lo (W)
    // Lower 8 bits of 11 bit frequency
    // Next 3 bit are in NR X4

    byte mNR14; // Only Bit 6 can be read.
    // Bit 7 - Initial (when set, sound restarts)
    // Bit 6 - Counter/consecutive selection
    // Bit 2-0 - Frequency's higher 3 bits

    byte mStatusBit;

    DutyUnit mDuty;
    SweepUnit mSweep;
    EnvelopeUnit mEnvelope;
    LengthCounter mLengthCounter;
};

#endif