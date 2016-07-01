/* 
 * File:   CPU.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 8:53 AM
 */

#ifndef CPU_H
#define CPU_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include "Common.h"
#include "EmulatorConfiguration.h"
#include "Debugger.h"
#include "Emulator.h"
#include "Memory.h"

using namespace std;

// Forward declaration of necessary classes
class Memory;
class Emulator;
class Debugger;

// Definition of the register file that contains each of the 8 registers
struct Registers
{
    // Use anonymous unions so we can access either an individual
    // byte or two bytes together
    union
    {
        struct
        {
            byte f;
            byte a;
        };
        word af;
    };

    union
    {
        struct
        {
            byte c;
            byte b;
        };
        word bc;
    };

    union
    {
        struct
        {
            byte e;
            byte d;
        };
        word de;
    };

    union
    {
        struct
        {
            byte l;
            byte h;
        };
        word hl;
    };
};

// Definition of the stack pointer. It acts like a single register.
// Use the union trick again so we can access either the complete
// word or an individual byte.
union StackPointer
{ 
    struct
    {
        byte lo;
        byte hi;
    };
    word reg;
};

// Flag definitions - used with the 'f' register
const int FLAG_ZERO  = 7; // Set when result is 0
const int FLAG_NEG   = 6; // Set when op is a subtraction
const int FLAG_HALF  = 5; // Set when overflow from lower nibble to upper nibble
const int FLAG_CARRY = 4; // Set when overflow occurs

class CPU 
{
public:
    
    /**
     * Create a new CPU.
     * 
     * @param emulator. A pointer to the hosting emulator.
     * @param memory.   A pointer to the memory unit for the emulated gameboy.
     * @param configuration. The system configuration.
     */
    CPU(Emulator* emulator, Memory* memory, EmulatorConfiguration* configuration);
    
    /**
     * Called during the emulation loop. Executes a single instruction.
     */
    void update();
    
    /**
     * Resets the registers, program counter, and stack pointer to their
     * default values. 
     */
    void reset();
    
    /**
     * Translates the instruction at the given address into a human readable
     * assembly string (e.g. JR NZ, 28AF).
     */
    string dissassembleInstruction(const word pc);
    
    void attachDebugger(Debugger* debugger) { mDebugger = debugger; }
    void setLogging(bool log)               { mLogging = log;       }
    
    word getProgramCounter() {return mProgramCounter;}
private:
    
    // Allow the debugger read only access to the state of the CPU
    friend class Debugger;
    
    Emulator* mEmulator;            // A pointer to the hosting emulator
    Memory* mMemory;                // A pointer to main memory
    Debugger* mDebugger;            // A pointer to the debugger (can be NULL)
    EmulatorConfiguration* mConfig; // A pointer to the emulator's current configuration
    
    Registers mRegisters;           // The registers for the CPU
    word mProgramCounter;           // Range is [0, 0xFFFF]
    StackPointer mStackPointer;     // The location of the top of the stack in memory
    byte mCurrentOpcode;            // The current opcode being executed   
    
    bool mInterruptMaster;          // True if interrupts are globally enabled
    bool mHalt;                     // True if the CPU has been halted
    bool mHaltBug;                  // Emulation of GB halt bug
    int mPendingInterruptEnabled;   // We have received an instruction to enable interrupts soon.
    int mPendingInterruptDisabled;  // We have received an instruction to disable interrupts soon.
    
    bool mLogging;
    
    /**
     * Called during the emulation loop. Gives the CPU a chance to handle
     * any interrupts that were requested since the last time they were handled.
     */
    void handleInterrupts();
    
    // Stack helper functions
    void pushWordOntoStack(word word);
    word popWordOffStack();
    
    // Instruction helpers
    void executeInstruction(byte opcode);
    void executeExtendedInstruction();
    byte getImmediateByte();
    word getImmediateWord();
    inline byte readByte(const word address);
    inline void writeByte(const word address, const byte data);
    inline void setFlag(int flag);
    inline void clearFlag(int flag);
    inline bool testFlag(int flag);
    
    // Instruction implementations   
    void CPU_LDHL();
    void CPU_8BIT_ADD(byte& reg, byte toAdd, bool useImmediate, bool addCarry);
    void CPU_8BIT_SUB(byte& reg, byte toSubtract, bool useImmediate, bool subCarry);
    void CPU_8BIT_AND(byte& reg, byte toAnd, bool useImmediate);
    void CPU_8BIT_OR(byte& reg, byte toOr, bool useImmediate);
    void CPU_8BIT_XOR(byte& reg, byte toXor, bool useImmediate);
    
    void CPU_8BIT_INC(byte& reg);
    void CPU_8BIT_INC(word address);
    void CPU_8BIT_DEC(byte& reg);
    void CPU_8BIT_DEC(word address);
    void CPU_16BIT_ADD(word source, word& dest);
    void CPU_16BIT_ADD(word& dest);
    
    void CPU_JUMP_IMMEDIATE(bool useCondition, int flag, bool condition);
    void CPU_JUMP(bool useCondition, int flag, bool condition);
    void CPU_CALL(bool useCondition, int flag, bool condition);
    void CPU_RETURN_CC(bool condition);
    void CPU_RETURN();
    void CPU_RESTART(word addr);
    
    void CPU_CPL();
    void CPU_CCF();
    void CPU_SCF();
    
    void CPU_RLC(byte& reg);
    void CPU_RL(byte& reg);
    void CPU_SLA(byte& reg);
    void CPU_RRC(byte& reg);
    void CPU_RR(byte& reg);
    void CPU_SRA(byte& reg);
    void CPU_SRL(byte& reg);
    
    void CPU_TEST_BIT(byte reg, int bit);
    void CPU_SWAP(byte& reg);
    void CPU_DECIMAL_ADJUST();
    
    void CPU_HALT();
    void CPU_STOP();
};

#endif /* CPU_H */

