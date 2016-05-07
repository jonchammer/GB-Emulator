#ifndef SOUND_UNIT4_H
#define SOUND_UNIT4_H

#include "../Sound.h"
#include "LFSR.h"
#include "EnvelopeUnit.h"
#include "LengthCounter.h"

class Sound;

/**
 * Produces white noise with an envelope.
 *
 * The linear feedback shift register (LFSR) generates a pseudo-random bit sequence. It has a 15-bit shift register with feedback. 
 * When clocked by the frequency timer, the low two bits (0 and 1) are XORed, all bits are shifted right by one, and the result of the XOR
 * is put into the now-empty high bit. If width mode is 1 (NR43), the XOR result is ALSO put into bit 6 AFTER the shift,
 * resulting in a 7-bit LFSR. The waveform output is bit 0 of the LFSR, INVERTED.
 *
 * LFSR has period of 2^n - 1 where n is LFSR register length in bits. LFSR produces exactly the same sequence every period. Gameboy LFSR can work as 
 * 7-bit (7 steps) or 15-bit (15 steps) LFSR. To optimize Sound unit 4 we can save bit sequence for 7 steps (LFSR7Table[127]) and 15 steps (LFSR15Table[32767]).
 */
class SoundUnit4
{
public:
    SoundUnit4(const bool& CGB, const bool skipBIOS, Sound& soundController);
    ~SoundUnit4();

    void timerStep(int clockDelta);
    void frameSequencerStep(byte sequencerStep);

    short getWaveLeftOutput();
    short getWaveRightOutput();

    void reset(const bool skipBIOS);

    byte getStatus()
    {
        return mStatusBit;
    }

    byte getNR41()
    {
        return mLengthCounter.getRNX1();
    }

    byte getNR42()
    {
        return mEnvelope.getNRX2();
    }

    byte getNR43()
    {
        return mlfsr.getNR43();
    }

    byte getNR44()
    {
        return mNR44 | 0xBF;
    }

    void NR41Changed(byte value, bool override = false);
    void NR42Changed(byte value, bool override = false);
    void NR43Changed(byte value, bool override = false);
    void NR44Changed(byte value, bool override = false);

    void NR52Changed(byte value);

private:

    const bool& mCGB;

    Sound& mSoundController;

    // Sound 4 I\O registers
    byte mNR44; // Only bit 6 can be read.
    // Bit 7 - Initial (when set, sound restarts)
    // Bit 6 - Counter/consecutive selection

    byte mStatusBit;

    LFSR mlfsr;
    EnvelopeUnit mEnvelope;
    LengthCounter mLengthCounter;
};

#endif