/* 
 * File:   Timers.h
 * Author: Jon C. Hammer
 *
 * Created on May 22, 2016, 3:43 PM
 */

#ifndef TIMERS_H
#define TIMERS_H

#include "Common.h"

// Forward Declarations
class Memory;

class Timers : public Component
{
public:
    
    // Constructors
    Timers(Memory* memory);
    
    // Memory access
    void write(const word address, const byte data);
    byte read(const word address) const;
    
    // State
    void update(int cycles);
    void reset();
    
private:
    
    Memory* mMemory;     // A pointer to the memory unit for this emulator
    int mTimerPeriod;    // Keeps track of the rate at which the timer updates
    int mTimerCounter;   // Current state of the timer
    int mDividerCounter; // Keeps track of the rate at which the divider register updates
    
    // Timer Registers
    byte mTAC;
    byte mTIMA;
    byte mTMA;
    byte mDividerReg;

    // Private functions to handle timing updates
    void handleDividerRegister(int cycles);
    void setClockFrequency();
};

#endif /* TIMERS_H */

