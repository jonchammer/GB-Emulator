/* 
 * File:   Debugger.h
 * Author: Jon C. Hammer
 *
 * Created on May 26, 2016, 10:27 PM
 */

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <unordered_set>
#include <list>
#include <algorithm>
#include <sstream>
#include "Common.h"
#include "CPU.h"
#include "Memory.h"
#include "Input.h"

using namespace std;

class CPU;
class Memory;
class Input;

struct Instruction
{
    word address;
    word romBank;
};

class Debugger
{
public:
    // Constructors / Destructors
    Debugger();
    
    // State update from other components
    void CPUUpdate();
    void CPUStackPush();
    void CPUStackPop();
    void joypadDown(const int key);
    void joypadUp(const int key);
    void memoryWrite(const word address, const byte data);
    void memoryRead(const word address, const byte data);
    
    // User interaction
    void setBreakpoint(const word pc);
    void setJoypadBreakpoint(const int button);
    void printStackTrace();
    void printLastInstructions();
    
    // Setters / Getters
    void setEnabled(bool enabled)      { mEnabled = enabled;       }
    void setPaused(bool paused)        { mPaused  = paused;        }
    void setNumLastInstructions(int n) { mNumLastInstructions = n; }
    void attachCPU(CPU* cpu)           { mCPU     = cpu;           }
    void attachMemory(Memory* memory)  { mMemory  = memory;        }
    void attachInput(Input* input)     { mInput   = input;         }
    
private:
    
    enum State {DEFAULT, STEP_OVER, STEP_OUT};
    
    CPU* mCPU;
    Memory* mMemory;
    Input* mInput;
    
    // Debug state
    bool mEnabled; // When true, the debugger will be operational.
    bool mPaused;  // When true, execution will freeze and will allow the user to step through the code.
    State mState;
    vector<Instruction> mStackTrace;      // Stores locations where the program counter changes drastically (e.g. calls/returns)
    bool mNextPush;                       // When true, the next instruction will be pushed onto the stack trace (rather than overwriting the last element)
    list<Instruction> mLastInstructions;  // Stores the sequence of instructions that have been executed.
    size_t mNumLastInstructions;          // Determines how many recent instructions are saved.
    
    vector<Instruction> mBreakpoints; // PC Breakpoints (e.g. 0xFFEE)
    byte mJoypadBreakpoints;                 // Break when a given button is pressed
    
    // Memory reads
    // Memory writes
    
    // Information to be printed
    // CPU state, Memory Reads, Memory Writes
    
    void executeCommand();
    void printState();
    bool hitBreakpoint(const word pc);
};

#endif /* DEBUGGER_H */

