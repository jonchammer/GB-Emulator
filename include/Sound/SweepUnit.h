#ifndef SWEEP_UNIT_H
#define SWEEP_UNIT_H

#include "../Common.h"
#include "DutyUnit.h"

/**
 * Sweep unit increments/decrements sound unit frequency.
 */
class SweepUnit
{
public:

    SweepUnit(byte& NRX3, byte& NRX4, byte& StatusBit, DutyUnit& dutyUnit) : mNRX3(NRX3), mNRX4(NRX4), mStatusBit(StatusBit), mDuty(dutyUnit)
    {
        reset();
    }

    void step(byte sequencerStep)
    {
        mCurrentSequencerStep = sequencerStep;

        //Sweep unit rate is 128 Hz (every 4 frame sequencer steps)
        if (mCurrentSequencerStep != 0 && mCurrentSequencerStep != 4)
            return;

        //Sweep time is positive
        if (mNRX0SweepPeriod)//Sweep function
        {
            mClockCounter--;
            if (mClockCounter <= 0)
            {
                mClockCounter += mNRX0SweepPeriod;

                if (mSweepEnabled)
                    sweepStep();
            }
        }
        
        //If sweep time is zero timer treats it as 8
        else
        {
            mClockCounter--;
            if (mClockCounter <= 0)
                mClockCounter += 8;
        }
    }

    void reset()
    {
        mNRX0         = 0;
        mOldNRX4      = 0;
        mClockCounter = 0;
        mShadowReg    = ((mNRX4 & 0x7) << 8) | mNRX3;
        mSweepEnabled = false;
    }

    void NRX0Changed(byte value)
    {
        mNRX0                 = value;
        mNRX0SweepPeriod      = (mNRX0 >> 4) & 0x7;
        mNRX0SweepDirection   = (mNRX0 >> 3) & 0x1;
        mNRX0SweepShiftLength = mNRX0 & 0x7;

        //Clearing negate mode after at least one calculation was made in negate mode disables channel 
        if (!mNRX0SweepDirection && mNegateMode)
            mStatusBit = 0;
    }

    byte getNRX0()
    {
        return mNRX0 | 0x80;
    }

    void NRX4Changed(byte value)
    {
        mOldNRX4 = value;

        if (value >> 7)
        {
            mShadowReg    = ((mNRX4 & 0x7) << 8) | mNRX3;
            mClockCounter = mNRX0SweepPeriod;
            mNegateMode   = false;

            //If sweep time is zero timer treats it as 8
            if (!mNRX0SweepPeriod)
                mClockCounter = 8;

            mSweepEnabled = mNRX0SweepShiftLength || mNRX0SweepPeriod;

            if (mNRX0SweepShiftLength)
                calculateFrequency();
        }
    }

private:

    void sweepStep()
    {
        int newFreq = calculateFrequency();

        if ((newFreq <= 2047.0) && mNRX0SweepShiftLength)
        {
            mNRX3 = newFreq & 0xFF;
            mNRX4 = (mNRX4 & 0xF8) | ((newFreq >> 8) & 0x7);

            mDuty.NRX3Changed(mNRX3);
            mDuty.NRX4Changed(mNRX4);

            mShadowReg = newFreq;

            //Frequency calculation and overflow check AGAIN but not saving new frequency
            calculateFrequency();
        }
    }

    int calculateFrequency()
    {
        int newFreq = mShadowReg;
        int shadowRegShift = mShadowReg >> mNRX0SweepShiftLength;

        // Decrease mode
        if (mNRX0SweepDirection)
        {
            //If next frequency is negative then using current frequency
            if (mShadowReg >= shadowRegShift)
                newFreq = mShadowReg - shadowRegShift;

            mNegateMode = true;
        } 
        
        // Increase mode
        else
        {
            newFreq = mShadowReg + shadowRegShift;
            mNegateMode = false;
        }

        // If frequency exceeds maximum, disable sound
        if (newFreq > 2047.0)
            mStatusBit = 0;

        return newFreq;
    }

    byte mNRX0; // Sweep register (R/W)
    // Bit 6-4 - Sweep Time
    // Bit 3 - Sweep Increase/Decrease
    //	0: Addition (frequency increases)
    //	1: Subtraction (frequency decreases)
    // Bit 2-0 - Number of sweep shift (n: 0-7)

    byte& mNRX3;
    byte mOldNRX4;
    byte& mNRX4;

    byte mNRX0SweepPeriod;
    byte mNRX0SweepDirection;
    byte mNRX0SweepShiftLength;
 
    int mShadowReg;
    int mClockCounter;
    bool mSweepEnabled;
    bool mNegateMode; //Indicates that at least one calculation was made in negate mode

    byte& mStatusBit;
    byte mCurrentSequencerStep;
    DutyUnit& mDuty;
};

#endif