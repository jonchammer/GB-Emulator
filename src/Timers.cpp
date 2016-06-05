#include "Timers.h"

Timers::Timers(Memory* memory, EmulatorConfiguration* configuration) : mMemory(memory), mConfig(configuration)
{
    // Attach this component to the memory at the correct locations
    mMemory->attachComponent(this, 0xFF04, 0xFF07);
    reset();
}
    
void Timers::write(const word address, const byte data)
{
    switch (address)
    {
        case TAC:
        {
            mTAC = data;
            setClockFrequency();
            break;
        }
        
        // Writing to the divider register resets it
        case DIVIDER_REGISTER: mDividerReg = 0;    break;
        case TIMA:             mTIMA       = data; break;
        case TMA:              mTMA        = data; break;
        
        default:
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to Timer." << endl;
        }
    }
}

byte Timers::read(const word address) const
{
    switch (address)
    {
        case DIVIDER_REGISTER: return mDividerReg;
        case TAC:              return mTAC;
        case TIMA:             return mTIMA;
        case TMA:              return mTMA;
        
        default:
        {
            cerr << "Address "; printHex(cerr, address); 
            cerr << " does not belong to Timer." << endl;
            return 0x00;
        }
    }
}

void Timers::update(int cycles)
{
    // In double speed mode, the timers operate twice as fast
    if (GBC(mConfig) && mConfig->doubleSpeed)
        cycles *= 2;
    
    handleDividerRegister(cycles);
    
    // The clock must be enabled for it to be updated
    // Bit 2 of TAC determines whether the clock is enabled or not
    if (testBit(mTAC, 2))
    {
        mTimerCounter += cycles;
        
        // The timer should be updated
        if (mTimerCounter >= mTimerPeriod)
        {
            int passedPeriods = mTimerCounter / mTimerPeriod;
            mTimerCounter %= mTimerPeriod;
            
            // Timer is about to overflow
            if (mTIMA + passedPeriods >= 255)
            {
                mTIMA = mTMA;
                mMemory->requestInterrupt(INTERRUPT_TIMER);
            }
            
            // Otherwise update the timer
            else mTIMA += passedPeriods;
        }
    }
}

void Timers::reset()
{
    // Initialize timing information
    mDividerCounter  = 0;
    mTimerPeriod     = 1024;
    mTimerCounter    = 0;
    
    // Reset the timing registers
    mDividerReg = 0x0;
    mTIMA       = 0x0;
    mTMA        = 0x0;
    mTAC        = 0x0;
    
//    if (skipBIOS)
//    {
//        // Do nothing. The registers are already initialized correctly.
//    }
}

void Timers::setClockFrequency()
{
    // The frequency is specified by the lower 2 bits of the TAC
    byte freq = mTAC & 0x3;
    switch (freq)
    {
        case 0 : mTimerPeriod = 1024; break; // Freq = 4096
        case 1 : mTimerPeriod = 16;   break; // Freq = 262144
        case 2 : mTimerPeriod = 64;   break; // Freq = 65536
        case 3 : mTimerPeriod = 256;  break; // Freq = 16382
    }
}

void Timers::handleDividerRegister(int cycles)
{
    mDividerCounter += cycles;
    if (mDividerCounter >= 255)
    {
        mDividerCounter = 0;
        mDividerReg++;
    }
}