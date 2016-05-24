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
using namespace std;

// Forward declaration of necessary classes
class Memory;
class Emulator;

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

class CPU 
{
public:
    
    /**
     * Create a new CPU.
     * 
     * @param emulator. A pointer to the hosting emulator.
     * @param memory.   A pointer to the memory unit for the emulated gameboy.
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    CPU(Emulator* emulator, Memory* memory, bool skipBIOS);
    
    /**
     * Called during the emulation loop. Executes a single instruction.
     */
    void update();
    
    /**
     * Resets the registers, program counter, and stack pointer to their
     * default values. 
     * 
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    void reset(bool skipBIOS);
    
    void setLogging(bool log) { mLogging = log;    }
    void toggleStepMode()     { mStep    = !mStep; }
    
private:
    Emulator* mEmulator;            // A pointer to the hosting emulator
    Memory* mMemory;                // A pointer to main memory
    Registers mRegisters;           // The registers for the CPU
    word mProgramCounter;           // Range is [0, 0xFFFF]
    StackPointer mStackPointer;     // The location of the top of the stack in memory
    
    bool mInterruptMaster;          // True if interrupts are globally enabled
    bool mHalt;                     // True if the CPU has been halted
    bool mHaltBug;                  // Emulation of GB halt bug
    int mPendingInterruptEnabled;   // We have received an instruction to enable interrupts soon.
    int mPendingInterruptDisabled;  // We have received an instruction to disable interrupts soon.
    
    bool mLogging;
    bool mStep;                     // When true, the user can step through execution one instruction at a time.
    
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
    void CPU_RESTART(byte n);
    
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
};

#endif /* CPU_H */

