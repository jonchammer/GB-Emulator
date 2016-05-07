/* 
 * File:   CPU.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 8:53 AM
 */

#include "CPU.h"

CPU::CPU(Memory* memory, bool skipBIOS) : mMemory(memory)
{
    reset(skipBIOS);
}

void CPU::reset(bool skipBIOS)
{
    // Initialize the PC
    mProgramCounter = 0x0;
    if (skipBIOS)
        mProgramCounter = 0x100;
    
    // Initialize the stack pointer
    mStackPointer.reg = 0xFFFE;
    
    // Initialize the registers
    mRegisters.af = 0x01B0;
    mRegisters.bc = 0x0013;
    mRegisters.de = 0x00D8;
    mRegisters.hl = 0x014D;
    
    // Initialize interrupt variables
    mInterruptMaster          = true;
    mHalt                     = false;
    mHaltBug                  = false;
    mPendingInterruptEnabled  = 0;
    mPendingInterruptDisabled = 0;
    
    mLogging = false;   
}

int CPU::executeNextOpcode()
{
    // When the program counter gets to 0x100, it's a sign that the BIOS has
    // completed. Tell the memory unit so it knows what to do.
    if (mProgramCounter == 0x100) mMemory->exitBIOS();
    
    byte opcode = mMemory->read(mProgramCounter);
    int  cycles = 4;
    
    if (mLogging)
    {
        cin.get();
        cout << "PC: "; printHex(cout, mProgramCounter); cout << endl;
        cout << "Op: "; printHex(cout, opcode); cout << endl;
        cout << "AF: "; printHex(cout, mRegisters.af); cout << endl;
        cout << "BC: "; printHex(cout, mRegisters.bc); cout << endl;
        cout << "DE: "; printHex(cout, mRegisters.de); cout << endl;
        cout << "HL: "; printHex(cout, mRegisters.hl); cout << endl;
        cout << "SP: "; printHex(cout, mStackPointer.reg); cout << endl;
        
        cout << "p1: "; printHex(cout, mMemory->read(mProgramCounter + 1)); cout << endl;
        cout << "p2: "; printHex(cout, mMemory->read(mProgramCounter + 2)); cout << endl;
        cout << endl;
    }

    if (!mHalt)
    {
        //cout << "Executing: "; printOpcode(cout, opcode); cout << endl;
        mProgramCounter++;
        cycles = executeInstruction(opcode);
    }
    
    // If we executed instruction 0xF3 or 0xFB, the program wants to enable
    // or disable interrupts. We have to wait one instruction before we can
    // handle the request, though.
    if (mPendingInterruptDisabled > 0)
    {
        mPendingInterruptDisabled--;
        if (mPendingInterruptDisabled == 0)
        {
            mPendingInterruptDisabled = false;
            mInterruptMaster          = false;
        }
    }
    
    if (mPendingInterruptEnabled > 0)
    {
        mPendingInterruptEnabled--;
        if (mPendingInterruptEnabled == 0)
        {
            mPendingInterruptEnabled = false;
            mInterruptMaster         = true;
        }
    }
    
    //-----------------
//    mProgramCounter++;
//    
//    if (!mHalt && mHaltBug)
//    {
//        mHaltBug = false;
//        mProgramCounter--;
//    }
//    
//    if (mPendingInterruptDisabled > 0)
//    {
//        if (mPendingInterruptDisabled == 1)
//            mInterruptMaster = false;
//        
//        mPendingInterruptDisabled--;
//    }
//    
//    if (mPendingInterruptEnabled > 0)
//    {
//        if (mPendingInterruptEnabled == 1)
//            mInterruptMaster = true;
//
//        mPendingInterruptEnabled--;
//    }
//    
//    cycles = executeInstruction(opcode);
    
    //------------------

    return cycles;
}

int CPU::handleInterrupts()
{
    byte req     = mMemory->read(INTERRUPT_REQUEST_REGISTER);
    byte enabled = mMemory->read(INTERRUPT_ENABLED_REGISTER);
    
    // Only do work if corresponding flags are set in both registers.
    // For interrupts, we only care about the lower 5 bits.
    if ((req & enabled) & 0x1F)
    {
        // An interrupt disables HALT, even if the interrupt is not serviced
        // because of the interrupt master flag.
        if (mHalt)
            mHalt = false;
        
        // Check the interrupt master flag to actually service the interrupt
        if (mInterruptMaster)
        {
            mInterruptMaster = false;
            pushWordOntoStack(mProgramCounter);
            
            // V Blank
            if (testBit(req, 0) && testBit(enabled, 0))
            {
                mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, 0));
                mProgramCounter = INTERRUPT_SERVICE_VBLANK;
                return 5;
            }
            
            // LCD
            else if (testBit(req, 1) && testBit(enabled, 1))
            {
                mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, 1));
                mProgramCounter = INTERRUPT_SERVICE_LCD;
                return 5;
            }
            
            // Timer
            else if (testBit(req, 2) && testBit(enabled, 2))
            {
                mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, 2));
                mProgramCounter = INTERRUPT_SERVICE_TIMER;
                return 5;
            }
            
            // Serial Transfer
            else if (testBit(req, 3) && testBit(enabled, 3))
            {
                mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, 3));
                mProgramCounter = INTERRUPT_SERVICE_SERIAL;
                return 5;
            }
            
            // Joypad
            else if (testBit(req, 4) && testBit(enabled, 4))
            {
                mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, 4));
                mProgramCounter = INTERRUPT_SERVICE_JOYPAD;
                return 5;
            }
        }
    }
    
    return 0;
}

void CPU::pushWordOntoStack(word word)
{
    byte hi = (word >> 8) & 0x00FF;
    byte lo = word & 0x00FF;
    mMemory->write(mStackPointer.reg - 1, hi);
    mMemory->write(mStackPointer.reg - 2, lo);
    mStackPointer.reg -= 2;
}

word CPU::popWordOffStack()
{
    word word = mMemory->read(mStackPointer.reg + 1) << 8;
    word     |= mMemory->read(mStackPointer.reg);
    mStackPointer.reg += 2;
    return word;
}

byte CPU::getImmediateByte()
{
    byte n = mMemory->read(mProgramCounter);
    mProgramCounter++;
    return n;
}

word CPU::getImmediateWord()
{
    word nn = mMemory->read(mProgramCounter + 1) << 8;
    nn |= mMemory->read(mProgramCounter);
    mProgramCounter += 2;
    return nn;
}

byte CPU::readByte(const word address)
{
    return mMemory->read(address);
}

void CPU::writeByte(const word address, const byte data)
{
    mMemory->write(address, data);
}

void CPU::setFlag(int flag)
{
    mRegisters.f = setBit(mRegisters.f, flag);
}

void CPU::clearFlag(int flag)
{
    mRegisters.f = clearBit(mRegisters.f, flag);
}
   
bool CPU::testFlag(int flag)
{
    return testBit(mRegisters.f, flag);
}

void CPU::CPU_LDHL()
{
    signedByte n  = (signedByte) getImmediateByte();
    mRegisters.hl = mStackPointer.reg + n;
    mRegisters.f  = 0x0;
    
    // Set carry based on the bottom byte (immediate answer must be interpreted as unsigned)
    int a = mStackPointer.reg & 0x00FF;
    int b = ((byte) n) & 0xFF;
    if (a + b > 0xFF)
        setFlag(FLAG_CARRY);
    else clearFlag(FLAG_CARRY);
    
    // Set half carry based on the bottom nibble (immediate answer must be interpreted as unsigned)
    a = a & 0x000F;
    b = b & 0x000F;
    if (a + b > 0x0F)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
}

// Add either the next byte in the instructions or the given byte (toAdd) and
// place the result in 'reg'.
void CPU::CPU_8BIT_ADD(byte& reg, byte toAdd, bool useImmediate, bool addCarry)
{
    byte before = reg;
    byte other  = toAdd;
    byte carry  = 0;
    
    // Are we adding immediate data or the second param?
    if (useImmediate)
        other = getImmediateByte();
     
    // Are we also adding the carry flag?
    if (addCarry && testFlag(FLAG_CARRY))
        carry = 0x01;
    
    reg += (other + carry);
    
    // Set the flags
    mRegisters.f = 0;
    if (reg == 0)
        setFlag(FLAG_ZERO);
    
    // Half carry
    word a = (word) (before & 0x0F);
    word b = (word) (other  & 0x0F);
    if (a + b + carry > 0x0F)
        setFlag(FLAG_HALF);
    
    // Full carry
    if ((unsigned int) before + (unsigned int) other + (unsigned int) carry > 0xFF)
        setFlag(FLAG_CARRY);
}

void CPU::CPU_8BIT_SUB(byte& reg, byte toSubtract, bool useImmediate, bool subCarry)
{
    byte before = reg;
    byte other  = toSubtract;
    byte carry  = 0;
    
    if (useImmediate)
        other = getImmediateByte();
    
    if (subCarry && testFlag(FLAG_CARRY))
        carry = 0x01;
    
    reg -= (other + carry);
    
    // Set flags
    mRegisters.f = 0x0;
    if (reg == 0)
        setFlag(FLAG_ZERO);
    setFlag(FLAG_NEG);
    
    if (before < (other + carry))
        setFlag(FLAG_CARRY);
    
    signedWord hTest = (before & 0x0F);
    hTest -= (other & 0x0F);
    hTest -= carry;
    if (hTest < 0)
        setFlag(FLAG_HALF);
}

void CPU::CPU_8BIT_AND(byte& reg, byte toAnd, bool useImmediate)
{
    byte other = toAnd;
    if (useImmediate)
        other = getImmediateByte();
    
    reg &= other;
    
    mRegisters.f = 0x0;
    setFlag(FLAG_HALF);
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_8BIT_OR(byte& reg, byte toOr, bool useImmediate)
{
    byte other = toOr;
    if (useImmediate)
        other = getImmediateByte();
    
    reg |= other;
    mRegisters.f = 0x0;
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_8BIT_XOR(byte& reg, byte toXor, bool useImmediate)
{
    byte other = toXor;
    if (useImmediate)
        other = getImmediateByte();
    
    reg ^= other;
    mRegisters.f = 0x0;
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_8BIT_INC(byte& reg)
{
    byte oldVal = reg;
    reg++;
    
    // Set flags
    if (reg == 0)
        setFlag(FLAG_ZERO);
    else clearFlag(FLAG_ZERO);
    
    clearFlag(FLAG_NEG);
    
    // If the low 4 bits were originally 1111, we have a half carry
    if ((oldVal & 0x0F) == 0x0F)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
}

void CPU::CPU_8BIT_INC(word address)
{
    byte oldVal = readByte(address);
    
    // If the low 4 bits were originally 1111, we have a half carry
    if ((oldVal & 0x0F) == 0x0F)
        setFlag(FLAG_HALF);
        
    else clearFlag(FLAG_HALF);
    
    // Do the increment
    oldVal++;
    
    // Set flags
    if (oldVal == 0)
        setFlag(FLAG_ZERO);
    else clearFlag(FLAG_ZERO);
    
    clearFlag(FLAG_NEG);
    
    // Save the result back to the original address
    writeByte(address, oldVal);
}

void CPU::CPU_8BIT_DEC(byte& reg)
{
    byte oldVal = reg;
    reg--;
    
    // Set flags
    if (reg == 0)
        setFlag(FLAG_ZERO);
    else clearFlag(FLAG_ZERO);
    
    setFlag(FLAG_NEG);
    
    if ((oldVal & 0x0F) == 0)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
}

void CPU::CPU_8BIT_DEC(word address)
{
    byte oldVal = readByte(address);
    
    if ((oldVal & 0x0F) == 0)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
    
    oldVal--;
    
    // Set flags
    if (oldVal == 0)
        setFlag(FLAG_ZERO);
    else clearFlag(FLAG_ZERO);
    
    setFlag(FLAG_NEG);
    
    // Save the result back to the original address
    writeByte(address, oldVal);
}

void CPU::CPU_16BIT_ADD(word source, word& dest)
{
    word orig = dest;
    dest += source;
    
    clearFlag(FLAG_NEG);
    int sum = (((int) orig) + source);
    if (sum > 0xFFFF)
        setFlag(FLAG_CARRY);
    else clearFlag(FLAG_CARRY);
    
    // To calculate the half carry, first figure out if there was
    // a carry between bits 7 and 8
    word c = 0;
    if ((int)(orig & 0xFF) + (int)(source & 0xFF) > 0xFF)
        c = 1;

    word a = (source & 0x0F00) >> 8;
    word b = (orig   & 0x0F00) >> 8;
    
    // If the sum of the bytes and the carry is larger than 1 byte, 
    // there was a half carry
    if (a + b + c > 0x0F)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
}

void CPU::CPU_16BIT_ADD(word& dest)
{
    word orig        = dest;
    signedByte toAdd = (signedByte) getImmediateByte();
    dest            += toAdd;
    mRegisters.f     = 0x0;
    
    // Set carry based on the bottom byte (immediate answer must be interpreted as unsigned)
    int a = orig & 0x00FF;
    int b = ((byte) toAdd) & 0xFF;
    if (a + b > 0xFF)
        setFlag(FLAG_CARRY);
    else clearFlag(FLAG_CARRY);
    
    // Set half carry based on the bottom nibble (immediate answer must be interpreted as unsigned)
    a = a & 0x000F;
    b = b & 0x000F;
    if (a + b > 0x0F)
        setFlag(FLAG_HALF);
    else clearFlag(FLAG_HALF);
}

void CPU::CPU_JUMP_IMMEDIATE(bool useCondition, int flag, bool condition)
{
    signedByte n = (signedByte) getImmediateByte();
    if (!useCondition || (testFlag(flag) == condition))
        mProgramCounter += n;
}

void CPU::CPU_JUMP(bool useCondition, int flag, bool condition)
{
    word address = getImmediateWord();
    if (!useCondition || (testFlag(flag) == condition))
        mProgramCounter = address;
}

void CPU::CPU_CALL(bool useCondition, int flag, bool condition)
{
    word nn = getImmediateWord();
   
    if (!useCondition || (testFlag(flag) == condition))
    {
        pushWordOntoStack(mProgramCounter);
        mProgramCounter = nn;
    }
}

void CPU::CPU_RETURN(bool useCondition, int flag, bool condition)
{
    if (!useCondition || (testFlag(flag) == condition))
    {
        mProgramCounter = popWordOffStack();
        return;
    }
}

void CPU::CPU_RESTART(byte n)
{
    pushWordOntoStack(mProgramCounter);
    mProgramCounter = n;
}

void CPU::CPU_RLC(byte& reg)
{
    // Correct for RLC, fails for RLCA
    mRegisters.f = 0x0;
    reg = (reg << 1) | (reg >> 7);
    
    // Set flags
    if (testBit(reg, 0))
        setFlag(FLAG_CARRY);
    
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_RL(byte& reg)
{
    bool carry   = testFlag(FLAG_CARRY);
    mRegisters.f = 0x0;
    
    // Copy b7 to carry
    if (testBit(reg, 7)) setFlag(FLAG_CARRY);
    
    reg <<= 1;
    reg = clearBit(reg, 0);
    
    // Copy carry into b0
    if (carry)    reg = setBit(reg, 0);
    if (reg == 0) setFlag(FLAG_ZERO);
}

void CPU::CPU_SLA(byte& reg)
{
    // Copy b7 into carry
    mRegisters.f = 0x0;
    if (testBit(reg, 7)) setFlag(FLAG_CARRY);
    
    reg <<= 1;
    clearBit(reg, 0);
    
    if (reg == 0)
        setFlag(FLAG_ZERO);
}
 
// Rotate right through carry
void CPU::CPU_RRC(byte& reg)
{
    bool isLSBSet = testBit(reg, 0);
    mRegisters.f  = 0x0;
    reg >>= 1;
    
    // Set flags
    if (isLSBSet)
    {
        setFlag(FLAG_CARRY);
        reg = setBit(reg, 7);
    }
    
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_RR(byte& reg)
{
    bool carry   = testFlag(FLAG_CARRY);
    mRegisters.f = 0x0;
    
    // Copy b0 to carry
    if (testBit(reg, 0)) setFlag(FLAG_CARRY);
    
    reg >>= 1;
    
    // Copy carry into b7
    if (carry)    reg = setBit(reg, 7);
    if (reg == 0) setFlag(FLAG_ZERO);
}

void CPU::CPU_SRA(byte& reg)
{
    // Save MSB
    bool msb = testBit(reg, 7);
    
    // Copy b0 into carry
    mRegisters.f = 0x0;
    if (testBit(reg, 0)) setFlag(FLAG_CARRY);
    
    reg >>= 1;
    reg = clearBit(reg, 7);
    
    // Copy MSB back into reg
    if (msb) reg = setBit(reg, 7);
    
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_SRL(byte& reg)
{
    // Copy b0 into carry
    mRegisters.f = 0x0;
    if (testBit(reg, 0)) setFlag(FLAG_CARRY);
    
    reg >>= 1;
    reg = clearBit(reg, 7);
    
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

// Sets the flags for a given byte
void CPU::CPU_TEST_BIT(byte reg, int bit)
{
    if (testBit(reg, bit))
        clearFlag(FLAG_ZERO);
    else setFlag(FLAG_ZERO);
    
    clearFlag(FLAG_NEG);
    setFlag(FLAG_HALF);
}
    
void CPU::CPU_CPL()
{
    mRegisters.a ^= 0xFF;
    setFlag(FLAG_NEG);
    setFlag(FLAG_HALF);
}

void CPU::CPU_CCF()
{
    clearFlag(FLAG_NEG);
    clearFlag(FLAG_HALF);
    mRegisters.f = toggleBit(mRegisters.f, FLAG_CARRY);
}

void CPU::CPU_SCF()
{
    clearFlag(FLAG_NEG);
    clearFlag(FLAG_HALF);
    setFlag(FLAG_CARRY);
}

void CPU::CPU_SWAP(byte& reg)
{
    byte lo = reg & 0x0F;
    byte hi = (reg & 0xF0) >> 4;
    reg     = lo << 4 | hi;
    
    mRegisters.f = 0x0;
    if (reg == 0)
        setFlag(FLAG_ZERO);
}

void CPU::CPU_DECIMAL_ADJUST()
{
    int a = mRegisters.a;
    if (!testFlag(FLAG_NEG))
    {
        if (testFlag(FLAG_HALF) || (a & 0xF) > 9)
            a += 0x06;
        
        if (testFlag(FLAG_CARRY) || a > 0x9F)
            a += 0x60;
    }
    else
    {
        if (testFlag(FLAG_HALF))
            a = (a - 6) & 0xFF;
        
        if (testFlag(FLAG_CARRY))
            a -= 0x60;
    }
    
    clearFlag(FLAG_HALF);
    clearFlag(FLAG_ZERO);
    
    if ((a & 0x100) == 0x100)
        setFlag(FLAG_CARRY);
    a &= 0xFF;
    
    if (a == 0)
        setFlag(FLAG_ZERO);
    mRegisters.a = (byte) a;
}

void CPU::CPU_HALT()
{
    if (!mHalt)
    {
        if (!mInterruptMaster && (mMemory->read(INTERRUPT_ENABLED_REGISTER) & mMemory->read(INTERRUPT_REQUEST_REGISTER) & 0x1F))
        {
            // if GCB, return 4 clock cycles
            
            // if GB...
            mHaltBug = true;
        }
        else
        {
            mHalt = true;
            mProgramCounter--;
        }
    }
    else mProgramCounter--;
}

// NOTE: All instruction implementations have been verified using test ROMs
// except for STOP. It is also unknown if the CPU timings are correct.
int CPU::executeInstruction(byte opcode)
{
    switch (opcode)
    {
        case 0x00: return 4;               // No-OP
        case 0x76: mHalt = true; return 4; // HALT
        case 0x10: return 4;               // STOP
        
        // Simple 8-bit loads
        case 0x3E: mRegisters.a = getImmediateByte();      return 8;
        case 0x06: mRegisters.b = getImmediateByte();      return 8;
        case 0x0E: mRegisters.c = getImmediateByte();      return 8;
        case 0x16: mRegisters.d = getImmediateByte();      return 8;
        case 0x1E: mRegisters.e = getImmediateByte();      return 8;
        case 0x26: mRegisters.h = getImmediateByte();      return 8;
        case 0x2E: mRegisters.l = getImmediateByte();      return 8; 
        case 0x7F: mRegisters.a = mRegisters.a;            return 4;
        case 0x78: mRegisters.a = mRegisters.b;            return 4;
        case 0x79: mRegisters.a = mRegisters.c;            return 4;
        case 0x7A: mRegisters.a = mRegisters.d;            return 4;
        case 0x7B: mRegisters.a = mRegisters.e;            return 4;
        case 0x7C: mRegisters.a = mRegisters.h;            return 4;
        case 0x7D: mRegisters.a = mRegisters.l;            return 4;
        case 0x47: mRegisters.b = mRegisters.a;            return 4;
        case 0x40: mRegisters.b = mRegisters.b;            return 4;
        case 0x41: mRegisters.b = mRegisters.c;            return 4;
        case 0x42: mRegisters.b = mRegisters.d;            return 4;
        case 0x43: mRegisters.b = mRegisters.e;            return 4;
        case 0x44: mRegisters.b = mRegisters.h;            return 4;
        case 0x45: mRegisters.b = mRegisters.l;            return 4;
        case 0x4F: mRegisters.c = mRegisters.a;            return 4;
        case 0x48: mRegisters.c = mRegisters.b;            return 4;
        case 0x49: mRegisters.c = mRegisters.c;            return 4;
        case 0x4A: mRegisters.c = mRegisters.d;            return 4;
        case 0x4B: mRegisters.c = mRegisters.e;            return 4;
        case 0x4C: mRegisters.c = mRegisters.h;            return 4;
        case 0x4D: mRegisters.c = mRegisters.l;            return 4;
        case 0x57: mRegisters.d = mRegisters.a;            return 4;
        case 0x50: mRegisters.d = mRegisters.b;            return 4;
        case 0x51: mRegisters.d = mRegisters.c;            return 4;
        case 0x52: mRegisters.d = mRegisters.d;            return 4;
        case 0x53: mRegisters.d = mRegisters.e;            return 4;
        case 0x54: mRegisters.d = mRegisters.h;            return 4;
        case 0x55: mRegisters.d = mRegisters.l;            return 4;
        case 0x5F: mRegisters.e = mRegisters.a;            return 4;
        case 0x58: mRegisters.e = mRegisters.b;            return 4;
        case 0x59: mRegisters.e = mRegisters.c;            return 4;
        case 0x5A: mRegisters.e = mRegisters.d;            return 4;
        case 0x5B: mRegisters.e = mRegisters.e;            return 4;
        case 0x5C: mRegisters.e = mRegisters.h;            return 4;
        case 0x5D: mRegisters.e = mRegisters.l;            return 4;
        case 0x67: mRegisters.h = mRegisters.a;            return 4;
        case 0x60: mRegisters.h = mRegisters.b;            return 4;
        case 0x61: mRegisters.h = mRegisters.c;            return 4;
        case 0x62: mRegisters.h = mRegisters.d;            return 4;
        case 0x63: mRegisters.h = mRegisters.e;            return 4;
        case 0x64: mRegisters.h = mRegisters.h;            return 4;
        case 0x65: mRegisters.h = mRegisters.l;            return 4;
        case 0x6F: mRegisters.l = mRegisters.a;            return 4;
        case 0x68: mRegisters.l = mRegisters.b;            return 4;
        case 0x69: mRegisters.l = mRegisters.c;            return 4;
        case 0x6A: mRegisters.l = mRegisters.d;            return 4;
        case 0x6B: mRegisters.l = mRegisters.e;            return 4;
        case 0x6C: mRegisters.l = mRegisters.h;            return 4;
        case 0x6D: mRegisters.l = mRegisters.l;            return 4;
        case 0x0A: mRegisters.a = readByte(mRegisters.bc); return 8;
        case 0x1A: mRegisters.a = readByte(mRegisters.de); return 8;
        case 0x7E: mRegisters.a = readByte(mRegisters.hl); return 8;
        case 0x46: mRegisters.b = readByte(mRegisters.hl); return 8;
        case 0x4E: mRegisters.c = readByte(mRegisters.hl); return 8;
        case 0x56: mRegisters.d = readByte(mRegisters.hl); return 8;
        case 0x5E: mRegisters.e = readByte(mRegisters.hl); return 8;
        case 0x66: mRegisters.h = readByte(mRegisters.hl); return 8;
        case 0x6E: mRegisters.l = readByte(mRegisters.hl); return 8;
        case 0x02: writeByte(mRegisters.bc, mRegisters.a); return 8;
        case 0x12: writeByte(mRegisters.de, mRegisters.a); return 8;
        case 0x77: writeByte(mRegisters.hl, mRegisters.a); return 8;
        case 0x70: writeByte(mRegisters.hl, mRegisters.b); return 8;
        case 0x71: writeByte(mRegisters.hl, mRegisters.c); return 8;
        case 0x72: writeByte(mRegisters.hl, mRegisters.d); return 8;
        case 0x73: writeByte(mRegisters.hl, mRegisters.e); return 8;
        case 0x74: writeByte(mRegisters.hl, mRegisters.h); return 8;
        case 0x75: writeByte(mRegisters.hl, mRegisters.l); return 8; 
        
        // Unusual 8-bit loads
        case 0x36: writeByte(mRegisters.hl,               getImmediateByte()); return 12;
        case 0xFA: mRegisters.a = readByte(getImmediateWord());                return 16;
        case 0xEA: writeByte(getImmediateWord(),          mRegisters.a);       return 16;
        case 0xF2: mRegisters.a = readByte(0xFF00 + mRegisters.c);             return 8;
        case 0xE2: writeByte(0xFF00 + mRegisters.c,       mRegisters.a);       return 8;
        case 0xE0: writeByte(0xFF00 + getImmediateByte(), mRegisters.a);       return 12;
        case 0xF0: mRegisters.a = readByte(0xFF00 + getImmediateByte());       return 12;   

        // Load Increment/Decrement
        case 0x3A: mRegisters.a = readByte(mRegisters.hl); mRegisters.hl--; return 8;
        case 0x2A: mRegisters.a = readByte(mRegisters.hl); mRegisters.hl++; return 8;
        case 0x32: writeByte(mRegisters.hl, mRegisters.a); mRegisters.hl--; return 8;
        case 0x22: writeByte(mRegisters.hl, mRegisters.a); mRegisters.hl++; return 8;
        
        // 16-bit loads
        case 0x01: mRegisters.bc     = getImmediateWord(); return 12;
        case 0x11: mRegisters.de     = getImmediateWord(); return 12;
        case 0x21: mRegisters.hl     = getImmediateWord(); return 12;
        case 0x31: mStackPointer.reg = getImmediateWord(); return 12;
        case 0xF9: mStackPointer.reg = mRegisters.hl;      return 8;
        case 0xF8: CPU_LDHL();                             return 12;
        case 0x08:{ word nn = getImmediateWord(); 
                   writeByte(nn, mStackPointer.lo); 
                   writeByte(nn + 1, mStackPointer.hi);    return 20;}
        
        // Stack Push/Pop
        case 0xF5: pushWordOntoStack(mRegisters.af);  return 16;
        case 0xC5: pushWordOntoStack(mRegisters.bc);  return 16;
        case 0xD5: pushWordOntoStack(mRegisters.de);  return 16;
        case 0xE5: pushWordOntoStack(mRegisters.hl);  return 16;
         
        case 0xF1: {mRegisters.af = popWordOffStack(); mRegisters.f &= 0xF0; return 12;} // We can't set the lower 4 bits of the flag register.
        case 0xC1: mRegisters.bc = popWordOffStack(); return 12;
        case 0xD1: mRegisters.de = popWordOffStack(); return 12;
        case 0xE1: mRegisters.hl = popWordOffStack(); return 12;
        
        // 8-Bit Addition
        case 0x87: CPU_8BIT_ADD(mRegisters.a, mRegisters.a, false, false);            return 4;
        case 0x80: CPU_8BIT_ADD(mRegisters.a, mRegisters.b, false, false);            return 4;
        case 0x81: CPU_8BIT_ADD(mRegisters.a, mRegisters.c, false, false);            return 4;
        case 0x82: CPU_8BIT_ADD(mRegisters.a, mRegisters.d, false, false);            return 4;
        case 0x83: CPU_8BIT_ADD(mRegisters.a, mRegisters.e, false, false);            return 4;
        case 0x84: CPU_8BIT_ADD(mRegisters.a, mRegisters.h, false, false);            return 4;
        case 0x85: CPU_8BIT_ADD(mRegisters.a, mRegisters.l, false, false);            return 4;
        case 0x86: CPU_8BIT_ADD(mRegisters.a, readByte(mRegisters.hl), false, false); return 8;
        case 0xC6: CPU_8BIT_ADD(mRegisters.a, 0x0, true, false);                      return 8;
        case 0x8F: CPU_8BIT_ADD(mRegisters.a, mRegisters.a, false, true);             return 4;
        case 0x88: CPU_8BIT_ADD(mRegisters.a, mRegisters.b, false, true);             return 4;
        case 0x89: CPU_8BIT_ADD(mRegisters.a, mRegisters.c, false, true);             return 4;
        case 0x8A: CPU_8BIT_ADD(mRegisters.a, mRegisters.d, false, true);             return 4;
        case 0x8B: CPU_8BIT_ADD(mRegisters.a, mRegisters.e, false, true);             return 4;
        case 0x8C: CPU_8BIT_ADD(mRegisters.a, mRegisters.h, false, true);             return 4;
        case 0x8D: CPU_8BIT_ADD(mRegisters.a, mRegisters.l, false, true);             return 4;
        case 0x8E: CPU_8BIT_ADD(mRegisters.a, readByte(mRegisters.hl), false, true);  return 8;
        case 0xCE: CPU_8BIT_ADD(mRegisters.a, 0x0, true, true);                       return 8;
        
        // 8-bit Subtraction
        case 0x97: CPU_8BIT_SUB(mRegisters.a, mRegisters.a, false, false);            return 4;
        case 0x90: CPU_8BIT_SUB(mRegisters.a, mRegisters.b, false, false);            return 4;
        case 0x91: CPU_8BIT_SUB(mRegisters.a, mRegisters.c, false, false);            return 4;
        case 0x92: CPU_8BIT_SUB(mRegisters.a, mRegisters.d, false, false);            return 4;
        case 0x93: CPU_8BIT_SUB(mRegisters.a, mRegisters.e, false, false);            return 4;
        case 0x94: CPU_8BIT_SUB(mRegisters.a, mRegisters.h, false, false);            return 4;
        case 0x95: CPU_8BIT_SUB(mRegisters.a, mRegisters.l, false, false);            return 4;
        case 0x96: CPU_8BIT_SUB(mRegisters.a, readByte(mRegisters.hl), false, false); return 8;
        case 0xD6: CPU_8BIT_SUB(mRegisters.a, 0x0, true, false);                      return 8;
        case 0x9F: CPU_8BIT_SUB(mRegisters.a, mRegisters.a, false, true);             return 4;
        case 0x98: CPU_8BIT_SUB(mRegisters.a, mRegisters.b, false, true);             return 4;
        case 0x99: CPU_8BIT_SUB(mRegisters.a, mRegisters.c, false, true);             return 4;
        case 0x9A: CPU_8BIT_SUB(mRegisters.a, mRegisters.d, false, true);             return 4;
        case 0x9B: CPU_8BIT_SUB(mRegisters.a, mRegisters.e, false, true);             return 4;
        case 0x9C: CPU_8BIT_SUB(mRegisters.a, mRegisters.h, false, true);             return 4;
        case 0x9D: CPU_8BIT_SUB(mRegisters.a, mRegisters.l, false, true);             return 4;
        case 0x9E: CPU_8BIT_SUB(mRegisters.a, readByte(mRegisters.hl), false, true);  return 8;
        case 0xDE: CPU_8BIT_SUB(mRegisters.a, 0x0, true, true);                       return 8;
        
        // 8-bit Logical AND
        case 0xA7: CPU_8BIT_AND(mRegisters.a, mRegisters.a, false);            return 4;
        case 0xA0: CPU_8BIT_AND(mRegisters.a, mRegisters.b, false);            return 4;
        case 0xA1: CPU_8BIT_AND(mRegisters.a, mRegisters.c, false);            return 4;
        case 0xA2: CPU_8BIT_AND(mRegisters.a, mRegisters.d, false);            return 4;
        case 0xA3: CPU_8BIT_AND(mRegisters.a, mRegisters.e, false);            return 4;
        case 0xA4: CPU_8BIT_AND(mRegisters.a, mRegisters.h, false);            return 4;
        case 0xA5: CPU_8BIT_AND(mRegisters.a, mRegisters.l, false);            return 4;
        case 0xA6: CPU_8BIT_AND(mRegisters.a, readByte(mRegisters.hl), false); return 8;
        case 0xE6: CPU_8BIT_AND(mRegisters.a, 0x0, true);                      return 8;
        
        // 8-bit Logical OR
        case 0xB7: CPU_8BIT_OR(mRegisters.a, mRegisters.a, false);            return 4;
        case 0xB0: CPU_8BIT_OR(mRegisters.a, mRegisters.b, false);            return 4;
        case 0xB1: CPU_8BIT_OR(mRegisters.a, mRegisters.c, false);            return 4;
        case 0xB2: CPU_8BIT_OR(mRegisters.a, mRegisters.d, false);            return 4;
        case 0xB3: CPU_8BIT_OR(mRegisters.a, mRegisters.e, false);            return 4;
        case 0xB4: CPU_8BIT_OR(mRegisters.a, mRegisters.h, false);            return 4;
        case 0xB5: CPU_8BIT_OR(mRegisters.a, mRegisters.l, false);            return 4;
        case 0xB6: CPU_8BIT_OR(mRegisters.a, readByte(mRegisters.hl), false); return 8;
        case 0xF6: CPU_8BIT_OR(mRegisters.a, 0x0, true);                      return 8;
        
        // 8-bit Logical XOR
        case 0xAF: CPU_8BIT_XOR(mRegisters.a, mRegisters.a, false);            return 4;
        case 0xA8: CPU_8BIT_XOR(mRegisters.a, mRegisters.b, false);            return 4;
        case 0xA9: CPU_8BIT_XOR(mRegisters.a, mRegisters.c, false);            return 4;
        case 0xAA: CPU_8BIT_XOR(mRegisters.a, mRegisters.d, false);            return 4;
        case 0xAB: CPU_8BIT_XOR(mRegisters.a, mRegisters.e, false);            return 4;
        case 0xAC: CPU_8BIT_XOR(mRegisters.a, mRegisters.h, false);            return 4;
        case 0xAD: CPU_8BIT_XOR(mRegisters.a, mRegisters.l, false);            return 4;
        case 0xAE: CPU_8BIT_XOR(mRegisters.a, readByte(mRegisters.hl), false); return 8;
        case 0xEE: CPU_8BIT_XOR(mRegisters.a, 0x0, true);                      return 8;
        
        // 8-bit Compare
        case 0xBF: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.a, false, false);            return 4;}
        case 0xB8: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.b, false, false);            return 4;}
        case 0xB9: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.c, false, false);            return 4;}
        case 0xBA: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.d, false, false);            return 4;}
        case 0xBB: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.e, false, false);            return 4;}
        case 0xBC: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.h, false, false);            return 4;}
        case 0xBD: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.l, false, false);            return 4;}
        case 0xBE: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, readByte(mRegisters.hl), false, false); return 8;}
        case 0xFE: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, 0x0, true, false);                      return 8;}
        
        // 8-bit Increment/Decrement
        case 0x3C: CPU_8BIT_INC(mRegisters.a);    return 4;
        case 0x04: CPU_8BIT_INC(mRegisters.b);    return 4;
        case 0x0C: CPU_8BIT_INC(mRegisters.c);    return 4;
        case 0x14: CPU_8BIT_INC(mRegisters.d);    return 4;
        case 0x1C: CPU_8BIT_INC(mRegisters.e);    return 4;
        case 0x24: CPU_8BIT_INC(mRegisters.h);    return 4;
        case 0x2C: CPU_8BIT_INC(mRegisters.l);    return 4;
        case 0x34: CPU_8BIT_INC(mRegisters.hl);   return 12;
        case 0x3D: CPU_8BIT_DEC(mRegisters.a);    return 4;
        case 0x05: CPU_8BIT_DEC(mRegisters.b);    return 4;
        case 0x0D: CPU_8BIT_DEC(mRegisters.c);    return 4;
        case 0x15: CPU_8BIT_DEC(mRegisters.d);    return 4;
        case 0x1D: CPU_8BIT_DEC(mRegisters.e);    return 4;
        case 0x25: CPU_8BIT_DEC(mRegisters.h);    return 4;
        case 0x2D: CPU_8BIT_DEC(mRegisters.l);    return 4;
        case 0x35: CPU_8BIT_DEC(mRegisters.hl);   return 12;
        
        // 16-bit Adddition
        case 0x09: CPU_16BIT_ADD(mRegisters.bc, mRegisters.hl);     return 8;
        case 0x19: CPU_16BIT_ADD(mRegisters.de, mRegisters.hl);     return 8;
        case 0x29: CPU_16BIT_ADD(mRegisters.hl, mRegisters.hl);     return 8;
        case 0x39: CPU_16BIT_ADD(mStackPointer.reg, mRegisters.hl); return 8;
        case 0xE8: CPU_16BIT_ADD(mStackPointer.reg);                return 16;
        
        // 16-bit Increment/Decrement
        case 0x03: mRegisters.bc++;               return 8;
        case 0x13: mRegisters.de++;               return 8;
        case 0x23: mRegisters.hl++;               return 8;
        case 0x33: mStackPointer.reg++;           return 8;
        case 0x0B: mRegisters.bc--;               return 8;
        case 0x1B: mRegisters.de--;               return 8;
        case 0x2B: mRegisters.hl--;               return 8;
        case 0x3B: mStackPointer.reg--;           return 8;
        
        // Misc
        case 0x27: CPU_DECIMAL_ADJUST();          return 4;
        case 0x2F: CPU_CPL();                     return 4;
        case 0x3F: CPU_CCF();                     return 4;
        case 0x37: CPU_SCF();                     return 4;
        case 0xF3: mPendingInterruptDisabled = 2; return 4;
        case 0xFB: mPendingInterruptEnabled  = 2; return 4;
        
        // Rotates/Shifts
        case 0x07: CPU_RLC(mRegisters.a); clearFlag(FLAG_ZERO); return 4; // RLCA != RLC A. RLCA always clears the zero flag.
        case 0x17: CPU_RL(mRegisters.a);  clearFlag(FLAG_ZERO); return 4; // Sim.
        case 0x0F: CPU_RRC(mRegisters.a); clearFlag(FLAG_ZERO); return 4;
        case 0x1F: CPU_RR(mRegisters.a);  clearFlag(FLAG_ZERO); return 4;
                
        // Jumps
        case 0xC3: CPU_JUMP(false, 0x0, false);                  return 12;
        case 0xC2: CPU_JUMP(true, FLAG_ZERO, false);             return 12;
        case 0xCA: CPU_JUMP(true, FLAG_ZERO, true);              return 12;
        case 0xD2: CPU_JUMP(true, FLAG_CARRY, false);            return 12;
        case 0xDA: CPU_JUMP(true, FLAG_CARRY, true);             return 12;
        case 0xE9: mProgramCounter = mRegisters.hl;              return 4;
        case 0x18: CPU_JUMP_IMMEDIATE(false, 0x0, false);        return 8;
        case 0x20: CPU_JUMP_IMMEDIATE(true, FLAG_ZERO, false);   return 8;
        case 0x28: CPU_JUMP_IMMEDIATE(true, FLAG_ZERO, true);    return 8;
        case 0x30: CPU_JUMP_IMMEDIATE(true, FLAG_CARRY, false);  return 8;
        case 0x38: CPU_JUMP_IMMEDIATE(true, FLAG_CARRY, true);   return 8;
        
        // Calls
        case 0xCD: CPU_CALL(false, 0x0, false);       return 12;
        case 0xC4: CPU_CALL(true, FLAG_ZERO, false);  return 12;
        case 0xCC: CPU_CALL(true, FLAG_ZERO, true);   return 12;
        case 0xD4: CPU_CALL(true, FLAG_CARRY, false); return 12;
        case 0xDC: CPU_CALL(true, FLAG_CARRY, true);  return 12;
        
        // Restarts
        case 0xC7: CPU_RESTART(0x00); return 32;
        case 0xCF: CPU_RESTART(0x08); return 32;
        case 0xD7: CPU_RESTART(0x10); return 32;
        case 0xDF: CPU_RESTART(0x18); return 32;
        case 0xE7: CPU_RESTART(0x20); return 32;
        case 0xEF: CPU_RESTART(0x28); return 32;
        case 0xF7: CPU_RESTART(0x30); return 32;
        case 0xFF: CPU_RESTART(0x38); return 32;
        
        // Returns
        case 0xC9: CPU_RETURN(false, 0x0, false);                          return 8;
        case 0xC0: CPU_RETURN(true, FLAG_ZERO, false);                     return 8;
        case 0xC8: CPU_RETURN(true, FLAG_ZERO, true);                      return 8;
        case 0xD0: CPU_RETURN(true, FLAG_CARRY, false);                    return 8;
        case 0xD8: CPU_RETURN(true, FLAG_CARRY, true);                     return 8;
        case 0xD9: CPU_RETURN(false, 0x0, false); mInterruptMaster = true; return 8;
        
        // Extended instructions
        case 0xCB: return executeExtendedInstruction();
        
        // Opcode not recognized
        default:
        {
            cerr << "Opcode not recognized: ";
            printHex(cerr, opcode);
            cerr << endl;
        }
    }
}

int CPU::executeExtendedInstruction()
{
    byte opcode = getImmediateByte();
    
    switch (opcode)
    {
        // Rotate reg left
        case 0x00: CPU_RLC(mRegisters.b);  return 8;
        case 0x01: CPU_RLC(mRegisters.c);  return 8;
        case 0x02: CPU_RLC(mRegisters.d);  return 8;
        case 0x03: CPU_RLC(mRegisters.e);  return 8;
        case 0x04: CPU_RLC(mRegisters.h);  return 8;
        case 0x05: CPU_RLC(mRegisters.l);  return 8;
        case 0x06: {byte tmp = readByte(mRegisters.hl); CPU_RLC(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x07: CPU_RLC(mRegisters.a);  return 8;
        
        // Rotate reg right
        case 0x08: CPU_RRC(mRegisters.b);  return 8;
        case 0x09: CPU_RRC(mRegisters.c);  return 8;
        case 0x0A: CPU_RRC(mRegisters.d);  return 8;
        case 0x0B: CPU_RRC(mRegisters.e);  return 8;
        case 0x0C: CPU_RRC(mRegisters.h);  return 8;
        case 0x0D: CPU_RRC(mRegisters.l);  return 8;
        case 0x0E: {byte tmp = readByte(mRegisters.hl); CPU_RRC(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x0F: CPU_RRC(mRegisters.a);  return 8;
        
        // Rotate reg left through carry
        case 0x10: CPU_RL(mRegisters.b);  return 8;
        case 0x11: CPU_RL(mRegisters.c);  return 8;
        case 0x12: CPU_RL(mRegisters.d);  return 8;
        case 0x13: CPU_RL(mRegisters.e);  return 8;
        case 0x14: CPU_RL(mRegisters.h);  return 8;
        case 0x15: CPU_RL(mRegisters.l);  return 8;
        case 0x16: {byte tmp = readByte(mRegisters.hl); CPU_RL(tmp);  writeByte(mRegisters.hl, tmp); return 16;}
        case 0x17: CPU_RL(mRegisters.a);  return 8;
        
        // Rotate reg right through carry
        case 0x18: CPU_RR(mRegisters.b);  return 8;
        case 0x19: CPU_RR(mRegisters.c);  return 8;
        case 0x1A: CPU_RR(mRegisters.d);  return 8;
        case 0x1B: CPU_RR(mRegisters.e);  return 8;
        case 0x1C: CPU_RR(mRegisters.h);  return 8;
        case 0x1D: CPU_RR(mRegisters.l);  return 8;
        case 0x1E: {byte tmp = readByte(mRegisters.hl); CPU_RR(tmp);  writeByte(mRegisters.hl, tmp); return 16;}
        case 0x1F: CPU_RR(mRegisters.a);  return 8;
        
        // Shift left into carry
        case 0x20: CPU_SLA(mRegisters.b);  return 8;
        case 0x21: CPU_SLA(mRegisters.c);  return 8;
        case 0x22: CPU_SLA(mRegisters.d);  return 8;
        case 0x23: CPU_SLA(mRegisters.e);  return 8;
        case 0x24: CPU_SLA(mRegisters.h);  return 8;
        case 0x25: CPU_SLA(mRegisters.l);  return 8;
        case 0x26: {byte tmp = readByte(mRegisters.hl); CPU_SLA(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x27: CPU_SLA(mRegisters.a);  return 8;
        
        // Shift right into carry
        case 0x28: CPU_SRA(mRegisters.b);  return 8;
        case 0x29: CPU_SRA(mRegisters.c);  return 8;
        case 0x2A: CPU_SRA(mRegisters.d);  return 8;
        case 0x2B: CPU_SRA(mRegisters.e);  return 8;
        case 0x2C: CPU_SRA(mRegisters.h);  return 8;
        case 0x2D: CPU_SRA(mRegisters.l);  return 8;
        case 0x2E: {byte tmp = readByte(mRegisters.hl); CPU_SRA(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x2F: CPU_SRA(mRegisters.a);  return 8;
        
        // Swap
        case 0x30: CPU_SWAP(mRegisters.b); return 8;
        case 0x31: CPU_SWAP(mRegisters.c); return 8;
        case 0x32: CPU_SWAP(mRegisters.d); return 8;
        case 0x33: CPU_SWAP(mRegisters.e); return 8;
        case 0x34: CPU_SWAP(mRegisters.h); return 8;
        case 0x35: CPU_SWAP(mRegisters.l); return 8;
        case 0x36: {byte tmp = readByte(mRegisters.hl); CPU_SWAP(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x37: CPU_SWAP(mRegisters.a); return 8;

        // Shift right logically into carry
        case 0x38: CPU_SRL(mRegisters.b);  return 8;
        case 0x39: CPU_SRL(mRegisters.c);  return 8;
        case 0x3A: CPU_SRL(mRegisters.d);  return 8;
        case 0x3B: CPU_SRL(mRegisters.e);  return 8;
        case 0x3C: CPU_SRL(mRegisters.h);  return 8;
        case 0x3D: CPU_SRL(mRegisters.l);  return 8;
        case 0x3E: {byte tmp = readByte(mRegisters.hl); CPU_SRL(tmp); writeByte(mRegisters.hl, tmp); return 16;}
        case 0x3F: CPU_SRL(mRegisters.a);  return 8;
       
        // Test bit
        case 0x40: CPU_TEST_BIT(mRegisters.b, 0);            return 8;
        case 0x41: CPU_TEST_BIT(mRegisters.c, 0);            return 8;
        case 0x42: CPU_TEST_BIT(mRegisters.d, 0);            return 8;
        case 0x43: CPU_TEST_BIT(mRegisters.e, 0);            return 8;
        case 0x44: CPU_TEST_BIT(mRegisters.h, 0);            return 8;
        case 0x45: CPU_TEST_BIT(mRegisters.l, 0);            return 8;
        case 0x46: CPU_TEST_BIT(readByte(mRegisters.hl), 0); return 16;
        case 0x47: CPU_TEST_BIT(mRegisters.a, 0);            return 8;
        case 0x48: CPU_TEST_BIT(mRegisters.b, 1);            return 8;
        case 0x49: CPU_TEST_BIT(mRegisters.c, 1);            return 8;
        case 0x4A: CPU_TEST_BIT(mRegisters.d, 1);            return 8;
        case 0x4B: CPU_TEST_BIT(mRegisters.e, 1);            return 8;
        case 0x4C: CPU_TEST_BIT(mRegisters.h, 1);            return 8;
        case 0x4D: CPU_TEST_BIT(mRegisters.l, 1);            return 8;
        case 0x4E: CPU_TEST_BIT(readByte(mRegisters.hl), 1); return 16;
        case 0x4F: CPU_TEST_BIT(mRegisters.a, 1);            return 8;
        case 0x50: CPU_TEST_BIT(mRegisters.b, 2);            return 8;
        case 0x51: CPU_TEST_BIT(mRegisters.c, 2);            return 8;
        case 0x52: CPU_TEST_BIT(mRegisters.d, 2);            return 8;
        case 0x53: CPU_TEST_BIT(mRegisters.e, 2);            return 8;
        case 0x54: CPU_TEST_BIT(mRegisters.h, 2);            return 8;
        case 0x55: CPU_TEST_BIT(mRegisters.l, 2);            return 8;
        case 0x56: CPU_TEST_BIT(readByte(mRegisters.hl), 2); return 16;
        case 0x57: CPU_TEST_BIT(mRegisters.a, 2);            return 8;
        case 0x58: CPU_TEST_BIT(mRegisters.b, 3);            return 8;
        case 0x59: CPU_TEST_BIT(mRegisters.c, 3);            return 8;
        case 0x5A: CPU_TEST_BIT(mRegisters.d, 3);            return 8;
        case 0x5B: CPU_TEST_BIT(mRegisters.e, 3);            return 8;
        case 0x5C: CPU_TEST_BIT(mRegisters.h, 3);            return 8;
        case 0x5D: CPU_TEST_BIT(mRegisters.l, 3);            return 8;
        case 0x5E: CPU_TEST_BIT(readByte(mRegisters.hl), 3); return 16;
        case 0x5F: CPU_TEST_BIT(mRegisters.a, 3);            return 8;
        case 0x60: CPU_TEST_BIT(mRegisters.b, 4);            return 8;
        case 0x61: CPU_TEST_BIT(mRegisters.c, 4);            return 8;
        case 0x62: CPU_TEST_BIT(mRegisters.d, 4);            return 8;
        case 0x63: CPU_TEST_BIT(mRegisters.e, 4);            return 8;
        case 0x64: CPU_TEST_BIT(mRegisters.h, 4);            return 8;
        case 0x65: CPU_TEST_BIT(mRegisters.l, 4);            return 8;
        case 0x66: CPU_TEST_BIT(readByte(mRegisters.hl), 4); return 16;
        case 0x67: CPU_TEST_BIT(mRegisters.a, 4);            return 8;
        case 0x68: CPU_TEST_BIT(mRegisters.b, 5);            return 8;
        case 0x69: CPU_TEST_BIT(mRegisters.c, 5);            return 8;
        case 0x6A: CPU_TEST_BIT(mRegisters.d, 5);            return 8;
        case 0x6B: CPU_TEST_BIT(mRegisters.e, 5);            return 8;
        case 0x6C: CPU_TEST_BIT(mRegisters.h, 5);            return 8;
        case 0x6D: CPU_TEST_BIT(mRegisters.l, 5);            return 8;
        case 0x6E: CPU_TEST_BIT(readByte(mRegisters.hl), 5); return 16;
        case 0x6F: CPU_TEST_BIT(mRegisters.a, 5);            return 8;
        case 0x70: CPU_TEST_BIT(mRegisters.b, 6);            return 8;
        case 0x71: CPU_TEST_BIT(mRegisters.c, 6);            return 8;
        case 0x72: CPU_TEST_BIT(mRegisters.d, 6);            return 8;
        case 0x73: CPU_TEST_BIT(mRegisters.e, 6);            return 8;
        case 0x74: CPU_TEST_BIT(mRegisters.h, 6);            return 8;
        case 0x75: CPU_TEST_BIT(mRegisters.l, 6);            return 8;
        case 0x76: CPU_TEST_BIT(readByte(mRegisters.hl), 6); return 16;
        case 0x77: CPU_TEST_BIT(mRegisters.a, 6);            return 8;
        case 0x78: CPU_TEST_BIT(mRegisters.b, 7);            return 8;
        case 0x79: CPU_TEST_BIT(mRegisters.c, 7);            return 8;
        case 0x7A: CPU_TEST_BIT(mRegisters.d, 7);            return 8;
        case 0x7B: CPU_TEST_BIT(mRegisters.e, 7);            return 8;
        case 0x7C: CPU_TEST_BIT(mRegisters.h, 7);            return 8;
        case 0x7D: CPU_TEST_BIT(mRegisters.l, 7);            return 8;
        case 0x7E: CPU_TEST_BIT(readByte(mRegisters.hl), 7); return 16;
        case 0x7F: CPU_TEST_BIT(mRegisters.a, 7);            return 8;
        
        // Reset Bit
        case 0x80: mRegisters.b = clearBit(mRegisters.b, 0);            return 8;
        case 0x81: mRegisters.c = clearBit(mRegisters.c, 0);            return 8;
        case 0x82: mRegisters.d = clearBit(mRegisters.d, 0);            return 8;
        case 0x83: mRegisters.e = clearBit(mRegisters.e, 0);            return 8;
        case 0x84: mRegisters.h = clearBit(mRegisters.h, 0);            return 8;
        case 0x85: mRegisters.l = clearBit(mRegisters.l, 0);            return 8;
        case 0x86: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 0)); return 16;}
        case 0x87: mRegisters.a = clearBit(mRegisters.a, 0);            return 8;
        case 0x88: mRegisters.b = clearBit(mRegisters.b, 1);            return 8;
        case 0x89: mRegisters.c = clearBit(mRegisters.c, 1);            return 8;
        case 0x8A: mRegisters.d = clearBit(mRegisters.d, 1);            return 8;
        case 0x8B: mRegisters.e = clearBit(mRegisters.e, 1);            return 8;
        case 0x8C: mRegisters.h = clearBit(mRegisters.h, 1);            return 8;
        case 0x8D: mRegisters.l = clearBit(mRegisters.l, 1);            return 8;
        case 0x8E: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 1)); return 16;}
        case 0x8F: mRegisters.a = clearBit(mRegisters.a, 1);            return 8;
        case 0x90: mRegisters.b = clearBit(mRegisters.b, 2);            return 8;
        case 0x91: mRegisters.c = clearBit(mRegisters.c, 2);            return 8;
        case 0x92: mRegisters.d = clearBit(mRegisters.d, 2);            return 8;
        case 0x93: mRegisters.e = clearBit(mRegisters.e, 2);            return 8;
        case 0x94: mRegisters.h = clearBit(mRegisters.h, 2);            return 8;
        case 0x95: mRegisters.l = clearBit(mRegisters.l, 2);            return 8;
        case 0x96: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 2)); return 16;}
        case 0x97: mRegisters.a = clearBit(mRegisters.a, 2);            return 8;
        case 0x98: mRegisters.b = clearBit(mRegisters.b, 3);            return 8;
        case 0x99: mRegisters.c = clearBit(mRegisters.c, 3);            return 8;
        case 0x9A: mRegisters.d = clearBit(mRegisters.d, 3);            return 8;
        case 0x9B: mRegisters.e = clearBit(mRegisters.e, 3);            return 8;
        case 0x9C: mRegisters.h = clearBit(mRegisters.h, 3);            return 8;
        case 0x9D: mRegisters.l = clearBit(mRegisters.l, 3);            return 8;
        case 0x9E: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 3)); return 16;}
        case 0x9F: mRegisters.a = clearBit(mRegisters.a, 3);            return 8;
        case 0xA0: mRegisters.b = clearBit(mRegisters.b, 4);            return 8;
        case 0xA1: mRegisters.c = clearBit(mRegisters.c, 4);            return 8;
        case 0xA2: mRegisters.d = clearBit(mRegisters.d, 4);            return 8;
        case 0xA3: mRegisters.e = clearBit(mRegisters.e, 4);            return 8;
        case 0xA4: mRegisters.h = clearBit(mRegisters.h, 4);            return 8;
        case 0xA5: mRegisters.l = clearBit(mRegisters.l, 4);            return 8;
        case 0xA6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 4)); return 16;}
        case 0xA7: mRegisters.a = clearBit(mRegisters.a, 4);            return 8;
        case 0xA8: mRegisters.b = clearBit(mRegisters.b, 5);            return 8;
        case 0xA9: mRegisters.c = clearBit(mRegisters.c, 5);            return 8;
        case 0xAA: mRegisters.d = clearBit(mRegisters.d, 5);            return 8;
        case 0xAB: mRegisters.e = clearBit(mRegisters.e, 5);             return 8;
        case 0xAC: mRegisters.h = clearBit(mRegisters.h, 5);            return 8;
        case 0xAD: mRegisters.l = clearBit(mRegisters.l, 5);            return 8;
        case 0xAE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 5)); return 16;}
        case 0xAF: mRegisters.a = clearBit(mRegisters.a, 5);            return 8;
        case 0xB0: mRegisters.b = clearBit(mRegisters.b, 6);            return 8;
        case 0xB1: mRegisters.c = clearBit(mRegisters.c, 6);            return 8;
        case 0xB2: mRegisters.d = clearBit(mRegisters.d, 6);            return 8;
        case 0xB3: mRegisters.e = clearBit(mRegisters.e, 6);            return 8;
        case 0xB4: mRegisters.h = clearBit(mRegisters.h, 6);            return 8;
        case 0xB5: mRegisters.l = clearBit(mRegisters.l, 6);            return 8;
        case 0xB6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 6)); return 16;}
        case 0xB7: mRegisters.a = clearBit(mRegisters.a, 6);            return 8;
        case 0xB8: mRegisters.b = clearBit(mRegisters.b, 7);            return 8;
        case 0xB9: mRegisters.c = clearBit(mRegisters.c, 7);            return 8;
        case 0xBA: mRegisters.d = clearBit(mRegisters.d, 7);            return 8;
        case 0xBB: mRegisters.e = clearBit(mRegisters.e, 7);            return 8;
        case 0xBC: mRegisters.h = clearBit(mRegisters.h, 7);            return 8;
        case 0xBD: mRegisters.l = clearBit(mRegisters.l, 7);            return 8;
        case 0xBE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 7)); return 16;}
        case 0xBF: mRegisters.a = clearBit(mRegisters.a, 7);            return 8;
        
        // Set Bit
        case 0xC0: mRegisters.b = setBit(mRegisters.b, 0);            return 8;
        case 0xC1: mRegisters.c = setBit(mRegisters.c, 0);            return 8;
        case 0xC2: mRegisters.d = setBit(mRegisters.d, 0);            return 8;
        case 0xC3: mRegisters.e = setBit(mRegisters.e, 0);            return 8;
        case 0xC4: mRegisters.h = setBit(mRegisters.h, 0);            return 8;
        case 0xC5: mRegisters.l = setBit(mRegisters.l, 0);            return 8;
        case 0xC6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 0)); return 16;}
        case 0xC7: mRegisters.a = setBit(mRegisters.a, 0);            return 8;
        case 0xC8: mRegisters.b = setBit(mRegisters.b, 1);            return 8;
        case 0xC9: mRegisters.c = setBit(mRegisters.c, 1);            return 8;
        case 0xCA: mRegisters.d = setBit(mRegisters.d, 1);            return 8;
        case 0xCB: mRegisters.e = setBit(mRegisters.e, 1);            return 8;
        case 0xCC: mRegisters.h = setBit(mRegisters.h, 1);            return 8;
        case 0xCD: mRegisters.l = setBit(mRegisters.l, 1);            return 8;
        case 0xCE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 1)); return 16;}
        case 0xCF: mRegisters.a = setBit(mRegisters.a, 1);            return 8;
        case 0xD0: mRegisters.b = setBit(mRegisters.b, 2);            return 8;
        case 0xD1: mRegisters.c = setBit(mRegisters.c, 2);            return 8;
        case 0xD2: mRegisters.d = setBit(mRegisters.d, 2);            return 8;
        case 0xD3: mRegisters.e = setBit(mRegisters.e, 2);            return 8;
        case 0xD4: mRegisters.h = setBit(mRegisters.h, 2);            return 8;
        case 0xD5: mRegisters.l = setBit(mRegisters.l, 2);            return 8;
        case 0xD6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 2)); return 16;}
        case 0xD7: mRegisters.a = setBit(mRegisters.a, 2);            return 8;
        case 0xD8: mRegisters.b = setBit(mRegisters.b, 3);            return 8;
        case 0xD9: mRegisters.c = setBit(mRegisters.c, 3);            return 8;
        case 0xDA: mRegisters.d = setBit(mRegisters.d, 3);            return 8;
        case 0xDB: mRegisters.e = setBit(mRegisters.e, 3);            return 8;
        case 0xDC: mRegisters.h = setBit(mRegisters.h, 3);            return 8;
        case 0xDD: mRegisters.l = setBit(mRegisters.l, 3);            return 8;
        case 0xDE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 3)); return 16;}
        case 0xDF: mRegisters.a = setBit(mRegisters.a, 3);            return 8;
        case 0xE0: mRegisters.b = setBit(mRegisters.b, 4);            return 8;
        case 0xE1: mRegisters.c = setBit(mRegisters.c, 4);            return 8;
        case 0xE2: mRegisters.d = setBit(mRegisters.d, 4);            return 8;
        case 0xE3: mRegisters.e = setBit(mRegisters.e, 4);            return 8;
        case 0xE4: mRegisters.h = setBit(mRegisters.h, 4);            return 8;
        case 0xE5: mRegisters.l = setBit(mRegisters.l, 4);            return 8;
        case 0xE6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 4)); return 16;}
        case 0xE7: mRegisters.a = setBit(mRegisters.a, 4);            return 8;
        case 0xE8: mRegisters.b = setBit(mRegisters.b, 5);            return 8;
        case 0xE9: mRegisters.c = setBit(mRegisters.c, 5);            return 8;
        case 0xEA: mRegisters.d = setBit(mRegisters.d, 5);            return 8;
        case 0xEB: mRegisters.e = setBit(mRegisters.e, 5);            return 8;
        case 0xEC: mRegisters.h = setBit(mRegisters.h, 5);            return 8;
        case 0xED: mRegisters.l = setBit(mRegisters.l, 5);            return 8;
        case 0xEE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 5)); return 16;}
        case 0xEF: mRegisters.a = setBit(mRegisters.a, 5);            return 8;
        case 0xF0: mRegisters.b = setBit(mRegisters.b, 6);            return 8;
        case 0xF1: mRegisters.c = setBit(mRegisters.c, 6);            return 8;
        case 0xF2: mRegisters.d = setBit(mRegisters.d, 6);            return 8;
        case 0xF3: mRegisters.e = setBit(mRegisters.e, 6);            return 8;
        case 0xF4: mRegisters.h = setBit(mRegisters.h, 6);            return 8;
        case 0xF5: mRegisters.l = setBit(mRegisters.l, 6);            return 8;
        case 0xF6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 6)); return 16;}
        case 0xF7: mRegisters.a = setBit(mRegisters.a, 6);            return 8;  
        case 0xF8: mRegisters.b = setBit(mRegisters.b, 7);            return 8;
        case 0xF9: mRegisters.c = setBit(mRegisters.c, 7);            return 8;
        case 0xFA: mRegisters.d = setBit(mRegisters.d, 7);            return 8;
        case 0xFB: mRegisters.e = setBit(mRegisters.e, 7);            return 8;
        case 0xFC: mRegisters.h = setBit(mRegisters.h, 7);            return 8;
        case 0xFD: mRegisters.l = setBit(mRegisters.l, 7);            return 8;
        case 0xFE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 7)); return 16;}
        case 0xFF: mRegisters.a = setBit(mRegisters.a, 7);            return 8;
        
        default:
        {
            cerr << "Extended opcode not recognized: ";
            printHex(cerr, (0xCB << 8 | opcode));
            cerr << endl;
        }
    }
}