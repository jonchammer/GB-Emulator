#ifndef DUTY_UNIT_H
#define DUTY_UNIT_H

#include "../Common.h"

class DutyUnit
{
public:

    /**
     * Create a new Duty Unit.
     */
    DutyUnit()
    {
        reset();
    }

    /**
     * Resets this Duty Unit to its default values.
     */
    void reset()
    {
        mClockCounter   = 0;
        mPeriod         = 2048 * 4;
        mOutput         = 0;
        mDutyPhase      = 0;
        mDutyCycleIndex = 0;
        mEnabled        = false;
    }

    void step(int clockDelta)
    {
        static const byte DutyCycles[4][8] = 
        {
            {0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 1, 1, 1},
            {0, 1, 1, 1, 1, 1, 1, 0}
        };

        mClockCounter += clockDelta;
        if (mClockCounter >= mPeriod)
        {
            int passedPeriods = mClockCounter / mPeriod;
            mClockCounter %= mPeriod;

            if (mEnabled)
                mDutyPhase = (mDutyPhase + passedPeriods) & 0x7;
            else
                mDutyPhase = 0;
            
            mOutput = DutyCycles[mDutyCycleIndex][mDutyPhase];
        }
    }

    int getOutput()
    {
        return mOutput;
    }

    void NRX1Changed(byte value)
    {
        mDutyCycleIndex = value >> 6;
    }

    void NRX3Changed(byte value)
    {
        NRX3 = value;

        calculatePeriod();
    }

    void NRX4Changed(byte value)
    {
        NRX4 = value;

        //Square duty sequence clocking is disabled until the first trigger
        if (NRX4 & 0x80)
        {
            mClockCounter = 0;
            mEnabled = true;
        }

        calculatePeriod();
    }

private:

    void calculatePeriod()
    {
        mPeriod = (2048 - (((NRX4 & 0x7) << 8) | NRX3)) * 4;
    }

    byte NRX3;
    byte NRX4;

    int mClockCounter;
    int mPeriod;
    int mOutput;
    int mDutyPhase;
    int mDutyCycleIndex;

    bool mEnabled;
};

#endif