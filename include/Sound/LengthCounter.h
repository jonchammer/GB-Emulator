#ifndef LENGTH_COUNTER_H
#define LENGTH_COUNTER_H

#include "../Common.h"

/**
 * Length counter controls sound unit state. When it reaches zero sound unit is stopped.
 */
class LengthCounter
{
public:

    LengthCounter(byte mask, byte &statusBit, int NRX1ReadMask = -1) : mMask(mask), mStatusBit(statusBit)
    {
        reset();

        if (NRX1ReadMask < 0)
            mNRX1ReadMask = mMask;
        
        else mNRX1ReadMask = NRX1ReadMask & 0xFF;
    }

    void step(byte sequencerStep)
    {
        mCurrentSequencerStep = sequencerStep;

        //Length counter rate is 256 Hz (every 2 frame sequencer steps)
        if (sequencerStep % 2)
            return;

        //Counter mode
        if (mNRX4CounterMode)
            clockLengthCounter();
    }

    void reset()
    {
        mNRX1                 = 0;
        mNRX4CounterMode      = 0;
        mClockCounter         = 0;
        mCurrentSequencerStep = 0;
    }

    void powerOFF()
    {
        mNRX1 &= mMask;
    }

    byte getLengthMask()
    {
        return mMask;
    }

    byte getRNX1()
    {
        return mNRX1 | mNRX1ReadMask;
    }

    void NRX1Changed(byte value)
    {
        mNRX1 = value;

        //Write to NRX1 resets length counter
        reloadLengthCounter();
    }

    void NRX4Changed(byte value)
    {
        //If Length counter previously was disabled and now enabled
        //If current frame sequencer step clocks length counter and length counter non-zero
        if (!mNRX4CounterMode && (value & 0x40) && !(mCurrentSequencerStep % 2) && mClockCounter)
        {
            byte oldStatusBit = mStatusBit;
            clockLengthCounter(); //extra length clock

            //Sound disabled only if trigger is clear
            if (value & 0x80)
                mStatusBit = oldStatusBit;
        }

        //If triggered and length counter reached zero
        if ((value & 0x80) && !mClockCounter)
        {
            NRX1Changed(mNRX1 & (~mMask)); //Loading max value to length counter

            //If length counter being enabled and current frame sequencer step clocks length counter
            if ((value & 0x40) && !(mCurrentSequencerStep % 2))
                clockLengthCounter(); //extra length clock
        }

        mNRX4 = value;
        mNRX4CounterMode = (mNRX4 >> 6) & 0x1;
    }

private:

    void reloadLengthCounter()
    {
        mClockCounter = (~mNRX1 & mMask) + 1;
    }

    void clockLengthCounter()
    {
        mClockCounter--;

        //Duration counter stops sound
        if (mClockCounter <= 0)
        {
            mClockCounter = 0;
            mStatusBit    = 0;
        }

        byte newNRX1 = ~(mClockCounter - 1) & mMask;
        mNRX1 = (mNRX1 & (~mMask)) | newNRX1;
    }

    byte mNRX1; //Sound length. Each sound unit has diffirent bit mapping
    byte mNRX4;

    byte mMask;
    byte mNRX1ReadMask;
    byte mNRX4CounterMode;
    byte& mStatusBit;
    int mClockCounter;

    byte mCurrentSequencerStep;
};

#endif