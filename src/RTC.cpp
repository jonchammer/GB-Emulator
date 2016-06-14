#include "RTC.h"
#include "Common.h"
#include <iostream>
#include <fstream>
using namespace std;

RTC::RTC() : mRTCReg(-1), mLastLatchWrite(0xFF), mBaseTime(0), mHaltTime(0)
{
    // Do nothing
}

bool RTC::loadState(const string& filename)
{
    ifstream din;
    din.open(filename.c_str(), ios::binary);
    if (!din) return false;
    
    din.read((char*) &mBaseTime, sizeof(mBaseTime));
    din.read((char*) &mHaltTime, sizeof(mHaltTime));
    din.read((char*) mRTCRegisters, 5);
        
    din.close();
    return true;
}

bool RTC::saveState(const string& filename)
{
    ofstream dout;
    dout.open(filename.c_str(), ios::out | ios::binary);
    if (!dout) return false;

    cout << "Base Time: " << mBaseTime << endl;
    cout << "Halt Time: " << mHaltTime << endl;
    for (int i = 0; i < 5; ++i)
        cout << "RTC Register " << i << " = " << (int) mRTCRegisters[i] << endl;

    dout.write((char*) &mBaseTime, sizeof(mBaseTime));
    dout.write((char*) &mHaltTime, sizeof(mHaltTime));
    dout.write((char*) mRTCRegisters, 5);
    
    dout.close();
    return true;
}
    
byte RTC::readFromActiveRegister()
{
    switch (mRTCReg)
    {
        // Read from one of the RTC registers
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C:
            return mRTCRegisters[mRTCReg - 0x08];
    }
    
    cerr << "Invalid RTC Register: " << mRTCReg << endl;
    return 0x00;
}

void RTC::updateActiveRegister(byte data)
{
    // Change one of the RTC registers
    switch (mRTCReg)
    {
        case 0x08: 
        {
            // Handle changes to the seconds
            time_t now                 = testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT) ? mHaltTime : time(NULL);
            mBaseTime                 += (now - mBaseTime) % 60;
            mBaseTime                 -= data;
            mRTCRegisters[RTC_SECONDS] = data;
            break;
        }
        case 0x09: 
        {
            // Handle changes to the minutes
            time_t oldBaseTime         = testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT) ? mHaltTime : time(NULL);
            time_t oldMinutes          = ((oldBaseTime - mBaseTime) / SECONDS_PER_MINUTE) % 60;
            mBaseTime                 += oldMinutes * SECONDS_PER_MINUTE;
            mBaseTime                 -= data * SECONDS_PER_MINUTE;
            mRTCRegisters[RTC_MINUTES] = data;
            break;
        }
        case 0x0A: 
        {
            // Handle changes to the hours
            time_t oldBaseTime       = testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT) ? mHaltTime : time(NULL);
            time_t oldHours          = ((oldBaseTime - mBaseTime) / SECONDS_PER_HOUR) % 24;
            mBaseTime               += oldHours * SECONDS_PER_HOUR;
            mBaseTime               -= data * SECONDS_PER_HOUR;
            mRTCRegisters[RTC_HOURS] = data;
            break;
        }
        case 0x0B:
        {
            // Handle changes to the lower 8 bits of the day
            time_t oldBaseTime          = testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT) ? mHaltTime : time(NULL);
            time_t oldLowDays           = ((oldBaseTime - mBaseTime) / SECONDS_PER_DAY) % 0xFF;
            mBaseTime                  += oldLowDays * SECONDS_PER_DAY;
            mBaseTime                  -= data * SECONDS_PER_DAY;
            mRTCRegisters[RTC_DAYS_LOW] = data;
            break;
        }
        case 0x0C:
        {
            // Record changes in the halt state of the clock
            bool currentlyHalted = testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT);
            bool willBeHalted    = testBit(data, FLAG_HALT);
            
            // Handle changes to the MSB of the day
            time_t oldBaseTime = currentlyHalted ? mHaltTime : time(NULL);
            time_t oldHighDays = ((oldBaseTime - mBaseTime) / SECONDS_PER_DAY) & 0x100;
            mBaseTime         += oldHighDays * SECONDS_PER_DAY;
            mBaseTime         -= ((data & 0x1) << 8) * SECONDS_PER_DAY;
            
            // We were halted, but we're proceeding now. Add however long we
            // were paused to the base time
            if (currentlyHalted && !willBeHalted)
                mBaseTime += time(NULL) - mHaltTime;
            
            // We are now halting. Store the current time as the halt time
            else if (!currentlyHalted && willBeHalted)
                mHaltTime = time(NULL);
            
            mRTCRegisters[RTC_DAYS_HIGH] = data;
            break;
        }
    }
}

void RTC::latch(byte data)
{
    if (mRTCReg != -1)
    {
        // Latching is enabled by writing a 0, and then writing a 1
        // to this address space
        if (mLastLatchWrite == 0x00 && data == 0x01)
        {
            time_t currentTime = (testBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_HALT) ? mHaltTime : time(NULL));
            time_t passedTime  = currentTime - mBaseTime;

            // If we've passed 511 (0x1FF) days, we have an overflow
            const long DAYS_511 = 0x1FF * SECONDS_PER_DAY;
            if (passedTime > DAYS_511)
            {
                do
                {
                    passedTime -= DAYS_511;
                    mBaseTime  += DAYS_511;
                }
                while (passedTime > DAYS_511);

                // Set the day counter overflow flag
                mRTCRegisters[RTC_DAYS_HIGH] = setBit(mRTCRegisters[RTC_DAYS_HIGH], FLAG_OVERFLOW); 
            }

            // Determine how many days have passed
            long daysPassed = passedTime / SECONDS_PER_DAY;
            mRTCRegisters[RTC_DAYS_LOW] = daysPassed & 0xFF;
            mRTCRegisters[RTC_DAYS_HIGH] &= 0xFE;
            mRTCRegisters[RTC_DAYS_HIGH] |= (daysPassed & 0x100) >> 8;
            passedTime -= daysPassed * SECONDS_PER_DAY;

            // Determine how many hours have passed
            long hoursPassed = passedTime / SECONDS_PER_HOUR;
            mRTCRegisters[RTC_HOURS] = (hoursPassed & 0xFF);
            passedTime -= hoursPassed * SECONDS_PER_HOUR;

            // Determine how many minutes have passed
            long minutesPassed = passedTime / SECONDS_PER_MINUTE;
            mRTCRegisters[RTC_MINUTES] = minutesPassed & 0xFF;
            passedTime -= minutesPassed * SECONDS_PER_MINUTE;

            // Whatever remains is how many seconds have passed
            mRTCRegisters[RTC_SECONDS] = passedTime & 0xFF;
        }
            
        mLastLatchWrite = data;
    }
}

void RTC::setActiveRegister(int reg)
{
    mRTCReg = reg;
}

int RTC::getActiveRegister()
{
    return mRTCReg;
}