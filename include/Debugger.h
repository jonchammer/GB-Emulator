/* 
 * File:   Debugger.h
 * Author: Jon C. Hammer
 *
 * Created on May 26, 2016, 10:27 PM
 */

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "Common.h"
#include <unordered_set>
#include <algorithm>
using namespace std;

class Debugger
{
public:
    // Constructors / Destructors
    Debugger();
    
    // State update from other components
    void CPUUpdate();
    void joypadUpdate();
    void memoryWrite(const word address, const byte data);
    void memoryRead(const word address, const byte data);
    
    // User interaction
    void setBreakpoint(const word pc);
    
    // Setters / Getters
    void setEnabled(bool enabled)     { mEnabled = enabled; }
    void attachCPU(CPU* cpu)          { mCPU     = cpu;     }
    void attachMemory(Memory* memory) { mMemory  = memory;  }
    void attachInput(Input* input)    { mInput   = input;   }
    
private:
    
    CPU* mCPU;
    Memory* mMemory;
    Input* mInput;
    
    // Debug state
    bool mEnabled; // When true, the debugger will be operational.
    bool mPaused;  // When true, execution will freeze and will allow the user to step through the code.
    
    unordered_set<word> mBreakpoints; // PC Breakpoints (e.g. 0xFFEE)
    // Memory reads
    // Memory writes
    // Joypad buttons
    
    // Information to be printed
    // CPU state, Memory Reads, Memory Writes
    
    void executeCommand();
    void printState();
};

#endif /* DEBUGGER_H */

