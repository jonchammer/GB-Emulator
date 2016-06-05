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

const word DIVIDER_REGISTER = 0xFF04; // Location of the divider register in memory
const word TIMA             = 0xFF05; // Location of timer in memory
const word TMA              = 0xFF06; // Location of timer reset value, which will probably be 0
const word TAC              = 0xFF07; // Location of timer frequency

class Timers : public Component
{
public:
    
    // Constructors
    Timers(Memory* memory, EmulatorConfiguration* configuration);
    virtual ~Timers() {}
    
    // Memory access
    void write(const word address, const byte data);
    byte read(const word address) const;
    
    // State
    void update(int cycles);
    void reset();
    
private:
    
    Memory* mMemory;     // A pointer to the memory unit for this emulator
    EmulatorConfiguration* mConfig;
    
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

