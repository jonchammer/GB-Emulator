#ifndef ENVELOPE_UNIT_H
#define ENVELOPE_UNIT_H

#include "../Common.h"

/**
 * This unit increments/decrements sound unit volume. When sound unit triggered, initial value (NRX2) is set as volume.
 * Once clocked it's decrements/increments volume. Volume can be in range of 0 to 15. Once it reaches 0 or 15 envelope unit stops.
 */
class EnvelopeUnit
{
public:

    /**
     * Create a new Envelope Unit.
     */
    EnvelopeUnit()
    {
        reset();
    }

    /**
     * Update this envelope (either incrementing or decrementing the volume
     * of the corresponding sound unit).
     */
    void step(byte sequencerStep)
    {
        mCurrentSequencerStep = sequencerStep;

        //Envelope unit rate is 64 Hz (every 8 frame sequencer steps)
        if (sequencerStep != 1) return;

        //Envelope function
        if (mNRX2Period)
        {
            mClockCounter--;

            if (mClockCounter <= 0)
            {
                mClockCounter += mNRX2Period;

                //Increase mode
                if (mNRX2Direction && mEnvelopeValue < 0xF0)
                    mEnvelopeValue++;
                
                //Decrease mode
                else if (!mNRX2Direction && mEnvelopeValue > 0)
                    mEnvelopeValue--;
            }
        }            
        
        //Envelope unit treats 0 period as 8
        else
        {
            mClockCounter--;
            if (mClockCounter <= 0)
                mClockCounter += 8;
        }
    }

    /**
     * Resets this Envelope Unit to its default values.
     */
    void reset()
    {
        mNRX2                 = 0;
        mEnvelopeValue        = 0;
        mClockCounter         = 0;
        mCurrentSequencerStep = 0;
    }

    byte getNRX2()
    {
        return mNRX2;
    }
    
    void NRX2Changed(byte value)
    {
        //"Zombie" mode
        if (!mNRX2Period && mClockCounter > 0)
            mEnvelopeValue++;
        else if (!mNRX2Direction)
            mEnvelopeValue += 2;

        //If the mode was changed (add to subtract or subtract to add), volume is set to 16 - volume
        if ((mNRX2 ^ value) & 0x8)
            mEnvelopeValue = 0x10 - mEnvelopeValue;

        mEnvelopeValue &= 0xF;
        mNRX2          = value;
        mNRX2Initial   = (mNRX2 >> 4) & 0xF;
        mNRX2Direction = (mNRX2 >> 3) & 0x1;
        mNRX2Period    = mNRX2 & 0x7;
        mDACState      = !(mNRX2Initial || mNRX2Direction);
    }

    void NRX4Changed(byte value)
    {
        if (value >> 7)
        {
            mClockCounter  = mNRX2Period;
            mEnvelopeValue = mNRX2Initial;

            //Envelope unit treats 0 period as 8
            if (!mClockCounter)
                mClockCounter = 8;

            //Next frame sequencer step clocks envelope unit
            if (mCurrentSequencerStep == 0)
                mClockCounter++;
        }
    }

    int getEnvelopeValue()
    {
        return mEnvelopeValue;
    }
    
    //If initial envelope value is 0 and envelope decrease - sound is turned OFF
    bool disablesSound()
    {
        return mDACState;
    }

private:
    byte mNRX2; // Envelope (R/W)
                // Bit 7-4 - Initial volume of envelope
                // Bit 3 - Envelope UP/DOWN
                //     0: Attenuate
                //     1: Amplify
                // Bit 2-0 - Number of envelope sweep (n: 0-7) (If zero, stop envelope operation.)

    byte mNRX2Initial;
    byte mNRX2Direction;
    byte mNRX2Period;
    bool mDACState;

    int mEnvelopeValue;
    int mClockCounter;

    byte mCurrentSequencerStep;
};

#endif