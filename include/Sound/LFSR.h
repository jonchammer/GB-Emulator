#ifndef LFSR_H
#define LFSR_H

#include "../Common.h"

class LFSR
{
public:

    /**
     * Create a new LFSR unit.
     */
    LFSR()
    {
        mLFSRPeriods[0] = 32768;
        mLFSRPeriods[1] = 128;

        reset();
    }

    /**
     * Advance this LSFR unit.
     */
    void step(int clockDelta)
    {
        static byte LFSR7Table[127] =
        {
            #include "LFSR7.inc"
        };
        static byte LFSR15Table[32767] =
        {
            #include "LFSR15.inc"
        };
        static byte* LFSRTables[2] = {LFSR15Table, LFSR7Table};

        //Using a noise channel clock shift of 14 or 15 results in the LFSR receiving no clocks
        if (mPeriod == 0 || mNR43Clock == 14 + 4 || mNR43Clock == 15 + 4)
            return;

        mClockCounter += clockDelta;
        if (mClockCounter >= mPeriod)
        {
            int passedPeriods = mClockCounter / mPeriod;
            mClockCounter    %= mPeriod;

            mSampleIndex += passedPeriods;
            mSampleIndex %= mLFSRPeriods[mNR43Step];

            mOutput = LFSRTables[mNR43Step][mSampleIndex];
        }
    }

    byte getOutput()
    {
        return mOutput;
    }

    /**
     * Resets this LSFR unit to its default state.
     */
    void reset()
    {
        NR43Changed(0);
        mClockCounter = 0;
        mSampleIndex  = 0;
        mOutput       = 0;
    }

    byte getNR43()
    {
        return mNR43;
    }

    void NR43Changed(byte value)
    {
        mNR43 = value;

        mNR43Clock = (mNR43 >> 4) + 4;
        mNR43Step  = (mNR43 >> 3) & 0x1;
        rNR43Ratio = mNR43 & 0x7;

        if (!rNR43Ratio)
        {
            rNR43Ratio = 1;
            mNR43Clock--;
        }

        mPeriod = rNR43Ratio << mNR43Clock;

        //Using a noise channel clock shift of 14 or 15 results in the LFSR receiving no clocks
        if (mPeriod == 0 || mNR43Clock == 14 + 4 || mNR43Clock == 15 + 4)
        {
            mClockCounter = 0;
            mSampleIndex  = 0;
        } 
        
        else mClockCounter %= mPeriod;
    }

    void NR44Changed(byte value)
    {
        if (value & 0x80)
        {
            mClockCounter = 0;
            mSampleIndex = 0;
        }
    }

private:

    byte mNR43; // Polynomial counter (R/W)
    // Bit 7-4 - Selection of the shift clock frequency of the polynomial counter
    // Bit 3 - Selection of the polynomial	counter's step
    // Bit 2-0 - Selection of the dividing ratio of frequencies:
    //	000: f * 1/2^3 * 2
    //	001: f * 1/2^3 * 1
    //	010: f * 1/2^3 * 1/2
    //	011: f * 1/2^3 * 1/3
    //	100: f * 1/2^3 * 1/4
    //	101: f * 1/2^3 * 1/5
    //	110: f * 1/2^3 * 1/6
    //	111: f * 1/2^3 * 1/7
    //	f = 4.194304 Mhz
    //
    // Selection of the polynomial counter step:
    //	0: 15 steps
    //	1: 7 steps
    //
    // Selection of the shift clock frequency of the polynomial counter:
    // 	0000: dividing ratio of frequencies * 1/2
    //	0001: dividing ratio of frequencies * 1/2^2
    //	0010: dividing ratio of frequencies * 1/2^3
    //	0011: dividing ratio of frequencies * 1/2^4
    //	: :
    //	: :
    //	: :
    //	0101: dividing ratio of frequencies * 1/2^14
    //	1110: prohibited code
    // 	1111: prohibited code
    byte mNR43Clock;
    byte mNR43Step;
    byte rNR43Ratio;

    byte mOutput;
    int mClockCounter;
    int mSampleIndex;
    int mPeriod;

    int mLFSRPeriods[2];
};

#endif