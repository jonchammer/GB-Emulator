/* 
 * File:   CPU.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 8:53 AM
 */

#include "CPU.h"

CPU::CPU(Emulator* emulator, Memory* memory, EmulatorConfiguration* configuration) : 
    mEmulator(emulator), mMemory(memory), mDebugger(NULL), mConfig(configuration)
{
    reset();
}

void CPU::reset()
{
    mProgramCounter   = 0x0;
    mStackPointer.reg = 0xFFFE;
    mRegisters.af     = 0x0000;
    mRegisters.bc     = 0x0000;
    mRegisters.de     = 0x0000;
    mRegisters.hl     = 0x0000;
    
    // Initialize interrupt variables
    mInterruptMaster          = false;
    mHalt                     = false;
    mHaltBug                  = false;
    mPendingInterruptEnabled  = 0;
    mPendingInterruptDisabled = 0;
    
    if (mConfig->skipBIOS)
    {
        mProgramCounter = 0x0100;
        mRegisters.af   = 0x01B0;
        mRegisters.bc   = 0x0013;
        mRegisters.de   = 0x00D8;
        mRegisters.hl   = 0x014D;
        
        if (GBC(mConfig))
            mRegisters.a = 0x11;
    }

    mLogging = false;
}

void CPU::update()
{
    mCurrentOpcode = readByte(mProgramCounter);
    
    // Allow the debugger to step in if necessary
    if (mDebugger != NULL) mDebugger->CPUUpdate();

    if (!mHalt)
    {
        mProgramCounter++;
        executeInstruction(mCurrentOpcode);
        
        // Emulation of the halt bug in the original gameboy. When a halt occurs
        // while interrupts are disabled, the following byte is repeated. For example,
        // HALT 0x2 0x4 will execute as HALT 0x2 0x2 0x4.
        if (mHaltBug)
        {
            mHaltBug = false;
            mProgramCounter--;
        }
    }
    
    // If we executed instruction 0xF3 or 0xFB, the program wants to enable
    // or disable interrupts. We have to wait one instruction before we can
    // handle the request, though.
    if (mPendingInterruptDisabled > 0)
    {
        mPendingInterruptDisabled--;
        if (mPendingInterruptDisabled == 0)
            mInterruptMaster = false;
    }
    
    if (mPendingInterruptEnabled > 0)
    {
        mPendingInterruptEnabled--;
        if (mPendingInterruptEnabled == 0)
            mInterruptMaster = true;
    }

    handleInterrupts();
}

void CPU::handleInterrupts()
{
    static word serviceRoutines[] = {INTERRUPT_SERVICE_VBLANK, INTERRUPT_SERVICE_LCD, 
        INTERRUPT_SERVICE_TIMER, INTERRUPT_SERVICE_SERIAL, INTERRUPT_SERVICE_JOYPAD};
    
    // We only care about the lower 5 bits of both registers, but we will
    // leave the upper bits alone in case they are being used elsewhere
    byte req     = mMemory->read(INTERRUPT_REQUEST_REGISTER);
    byte enabled = mMemory->read(INTERRUPT_ENABLED_REGISTER);
    
    if (req & 0x1F)
    {
        // An interrupt disables HALT, even if the interrupt is not serviced
        // because of the interrupt master flag.
        mHalt = false;
        
        if (mInterruptMaster && (req & enabled & 0x1F))
        {
            // Go through each interrupt in order of priority
            for (int i = 0; i < 5; ++i)
            {
                if (testBit(req, i) && testBit(enabled, i))
                {
                    mInterruptMaster = false;
                    
                    // Clear the interrupt bit since we've handled the request
                    mMemory->write(INTERRUPT_REQUEST_REGISTER, clearBit(req, i));
                    
                    // Save current execution state
                    if (mDebugger != NULL) mDebugger->CPUStackPush();
                    pushWordOntoStack(mProgramCounter);
                    
                    // Go to the proper interrupt service routine
                    mProgramCounter = serviceRoutines[i];
                    mEmulator->sync(8);
                    
                    break;
                }
            }
        }
    }
}

void CPU::pushWordOntoStack(word word)
{
    byte hi = (word >> 8) & 0x00FF;
    byte lo = word & 0x00FF;
    
    mStackPointer.reg--;
    mMemory->write(mStackPointer.reg, hi);
    mEmulator->sync(4);
    
    mStackPointer.reg--;
    mMemory->write(mStackPointer.reg, lo);
    mEmulator->sync(4);
    
    mEmulator->sync(4);
}

word CPU::popWordOffStack()
{
    word result = mMemory->read(mStackPointer.reg);
    mEmulator->sync(4);
    mStackPointer.reg++;
    
    result |= mMemory->read(mStackPointer.reg) << 8;
    mEmulator->sync(4);
    mStackPointer.reg++;
    
    return result;
}

byte CPU::getImmediateByte()
{
    byte n = mMemory->read(mProgramCounter);
    mEmulator->sync(4);
    
    mProgramCounter++;
    return n;
}

word CPU::getImmediateWord()
{
    word nn = mMemory->read(mProgramCounter + 1) << 8;
    mEmulator->sync(4);
    
    nn |= mMemory->read(mProgramCounter);
    mEmulator->sync(4);
    
    mProgramCounter += 2;
    return nn;
}

byte CPU::readByte(const word address)
{
    byte value = mMemory->read(address);
    if (mLogging && false)
    {
        cout << "Read. ["; 
        printHex(cout, address); 
        cout << "] = "; 
        printHex(cout, value); 
        cout << endl;
    }
    
    mEmulator->sync(4);
    return value;
}

void CPU::writeByte(const word address, const byte data)
{
    mMemory->write(address, data);
    mEmulator->sync(4);
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

// NOTE: All instruction implementations have been verified using test ROMs
// except for STOP.
void CPU::executeInstruction(byte opcode)
{
    switch (opcode)
    {
        case 0x00: return;               // No-OP
        case 0x76: CPU_HALT(); return;   // HALT
        case 0x10: CPU_STOP(); return;   // STOP
        
        // Simple 8-bit loads
        case 0x3E: mRegisters.a = getImmediateByte();      return;
        case 0x06: mRegisters.b = getImmediateByte();      return;
        case 0x0E: mRegisters.c = getImmediateByte();      return;
        case 0x16: mRegisters.d = getImmediateByte();      return;
        case 0x1E: mRegisters.e = getImmediateByte();      return;
        case 0x26: mRegisters.h = getImmediateByte();      return;
        case 0x2E: mRegisters.l = getImmediateByte();      return; 
        case 0x7F: mRegisters.a = mRegisters.a;            return;
        case 0x78: mRegisters.a = mRegisters.b;            return;
        case 0x79: mRegisters.a = mRegisters.c;            return;
        case 0x7A: mRegisters.a = mRegisters.d;            return;
        case 0x7B: mRegisters.a = mRegisters.e;            return;
        case 0x7C: mRegisters.a = mRegisters.h;            return;
        case 0x7D: mRegisters.a = mRegisters.l;            return;
        case 0x47: mRegisters.b = mRegisters.a;            return;
        case 0x40: mRegisters.b = mRegisters.b;            return;
        case 0x41: mRegisters.b = mRegisters.c;            return;
        case 0x42: mRegisters.b = mRegisters.d;            return;
        case 0x43: mRegisters.b = mRegisters.e;            return;
        case 0x44: mRegisters.b = mRegisters.h;            return;
        case 0x45: mRegisters.b = mRegisters.l;            return;
        case 0x4F: mRegisters.c = mRegisters.a;            return;
        case 0x48: mRegisters.c = mRegisters.b;            return;
        case 0x49: mRegisters.c = mRegisters.c;            return;
        case 0x4A: mRegisters.c = mRegisters.d;            return;
        case 0x4B: mRegisters.c = mRegisters.e;            return;
        case 0x4C: mRegisters.c = mRegisters.h;            return;
        case 0x4D: mRegisters.c = mRegisters.l;            return;
        case 0x57: mRegisters.d = mRegisters.a;            return;
        case 0x50: mRegisters.d = mRegisters.b;            return;
        case 0x51: mRegisters.d = mRegisters.c;            return;
        case 0x52: mRegisters.d = mRegisters.d;            return;
        case 0x53: mRegisters.d = mRegisters.e;            return;
        case 0x54: mRegisters.d = mRegisters.h;            return;
        case 0x55: mRegisters.d = mRegisters.l;            return;
        case 0x5F: mRegisters.e = mRegisters.a;            return;
        case 0x58: mRegisters.e = mRegisters.b;            return;
        case 0x59: mRegisters.e = mRegisters.c;            return;
        case 0x5A: mRegisters.e = mRegisters.d;            return;
        case 0x5B: mRegisters.e = mRegisters.e;            return;
        case 0x5C: mRegisters.e = mRegisters.h;            return;
        case 0x5D: mRegisters.e = mRegisters.l;            return;
        case 0x67: mRegisters.h = mRegisters.a;            return;
        case 0x60: mRegisters.h = mRegisters.b;            return;
        case 0x61: mRegisters.h = mRegisters.c;            return;
        case 0x62: mRegisters.h = mRegisters.d;            return;
        case 0x63: mRegisters.h = mRegisters.e;            return;
        case 0x64: mRegisters.h = mRegisters.h;            return;
        case 0x65: mRegisters.h = mRegisters.l;            return;
        case 0x6F: mRegisters.l = mRegisters.a;            return;
        case 0x68: mRegisters.l = mRegisters.b;            return;
        case 0x69: mRegisters.l = mRegisters.c;            return;
        case 0x6A: mRegisters.l = mRegisters.d;            return;
        case 0x6B: mRegisters.l = mRegisters.e;            return;
        case 0x6C: mRegisters.l = mRegisters.h;            return;
        case 0x6D: mRegisters.l = mRegisters.l;            return;
        case 0x0A: mRegisters.a = readByte(mRegisters.bc); return;
        case 0x1A: mRegisters.a = readByte(mRegisters.de); return;
        case 0x7E: mRegisters.a = readByte(mRegisters.hl); return;
        case 0x46: mRegisters.b = readByte(mRegisters.hl); return;
        case 0x4E: mRegisters.c = readByte(mRegisters.hl); return;
        case 0x56: mRegisters.d = readByte(mRegisters.hl); return;
        case 0x5E: mRegisters.e = readByte(mRegisters.hl); return;
        case 0x66: mRegisters.h = readByte(mRegisters.hl); return;
        case 0x6E: mRegisters.l = readByte(mRegisters.hl); return;
        case 0x02: writeByte(mRegisters.bc, mRegisters.a); return;
        case 0x12: writeByte(mRegisters.de, mRegisters.a); return;
        case 0x77: writeByte(mRegisters.hl, mRegisters.a); return;
        case 0x70: writeByte(mRegisters.hl, mRegisters.b); return;
        case 0x71: writeByte(mRegisters.hl, mRegisters.c); return;
        case 0x72: writeByte(mRegisters.hl, mRegisters.d); return;
        case 0x73: writeByte(mRegisters.hl, mRegisters.e); return;
        case 0x74: writeByte(mRegisters.hl, mRegisters.h); return;
        case 0x75: writeByte(mRegisters.hl, mRegisters.l); return; 
        
        // Unusual 8-bit loads
        case 0x36: writeByte(mRegisters.hl,               getImmediateByte()); return;
        case 0xFA: mRegisters.a = readByte(getImmediateWord());                return;
        case 0xEA: writeByte(getImmediateWord(),          mRegisters.a);       return;
        case 0xF2: mRegisters.a = readByte(0xFF00 + mRegisters.c);             return;
        case 0xE2: writeByte(0xFF00 + mRegisters.c,       mRegisters.a);       return;
        case 0xE0: writeByte(0xFF00 + getImmediateByte(), mRegisters.a);       return;
        case 0xF0: mRegisters.a = readByte(0xFF00 + getImmediateByte());       return;   

        // Load Increment/Decrement
        case 0x3A: mRegisters.a = readByte(mRegisters.hl); mRegisters.hl--; return;
        case 0x2A: mRegisters.a = readByte(mRegisters.hl); mRegisters.hl++; return;
        case 0x32: writeByte(mRegisters.hl, mRegisters.a); mRegisters.hl--; return;
        case 0x22: writeByte(mRegisters.hl, mRegisters.a); mRegisters.hl++; return;
        
        // 16-bit loads
        case 0x01: mRegisters.bc     = getImmediateWord();                return;
        case 0x11: mRegisters.de     = getImmediateWord();                return;
        case 0x21: mRegisters.hl     = getImmediateWord();                return;
        case 0x31: mStackPointer.reg = getImmediateWord();                return;
        case 0xF9: mStackPointer.reg = mRegisters.hl; mEmulator->sync(4); return;
        case 0xF8: CPU_LDHL();                                            return;
        case 0x08:
        { 
            word nn = getImmediateWord(); 
            writeByte(nn, mStackPointer.lo); 
            writeByte(nn + 1, mStackPointer.hi);    
            return;
        }
        
        // Stack Push/Pop
        case 0xF5: pushWordOntoStack(mRegisters.af);  return;
        case 0xC5: pushWordOntoStack(mRegisters.bc);  return;
        case 0xD5: pushWordOntoStack(mRegisters.de);  return;
        case 0xE5: pushWordOntoStack(mRegisters.hl);  return;
         
        case 0xF1: {mRegisters.af = popWordOffStack(); mRegisters.f &= 0xF0; return;} // We can't set the lower 4 bits of the flag register.
        case 0xC1: mRegisters.bc = popWordOffStack(); return;
        case 0xD1: mRegisters.de = popWordOffStack(); return;
        case 0xE1: mRegisters.hl = popWordOffStack(); return;
        
        // 8-Bit Addition
        case 0x87: CPU_8BIT_ADD(mRegisters.a, mRegisters.a, false, false);            return;
        case 0x80: CPU_8BIT_ADD(mRegisters.a, mRegisters.b, false, false);            return;
        case 0x81: CPU_8BIT_ADD(mRegisters.a, mRegisters.c, false, false);            return;
        case 0x82: CPU_8BIT_ADD(mRegisters.a, mRegisters.d, false, false);            return;
        case 0x83: CPU_8BIT_ADD(mRegisters.a, mRegisters.e, false, false);            return;
        case 0x84: CPU_8BIT_ADD(mRegisters.a, mRegisters.h, false, false);            return;
        case 0x85: CPU_8BIT_ADD(mRegisters.a, mRegisters.l, false, false);            return;
        case 0x86: CPU_8BIT_ADD(mRegisters.a, readByte(mRegisters.hl), false, false); return;
        case 0xC6: CPU_8BIT_ADD(mRegisters.a, 0x0, true, false);                      return;
        case 0x8F: CPU_8BIT_ADD(mRegisters.a, mRegisters.a, false, true);             return;
        case 0x88: CPU_8BIT_ADD(mRegisters.a, mRegisters.b, false, true);             return;
        case 0x89: CPU_8BIT_ADD(mRegisters.a, mRegisters.c, false, true);             return;
        case 0x8A: CPU_8BIT_ADD(mRegisters.a, mRegisters.d, false, true);             return;
        case 0x8B: CPU_8BIT_ADD(mRegisters.a, mRegisters.e, false, true);             return;
        case 0x8C: CPU_8BIT_ADD(mRegisters.a, mRegisters.h, false, true);             return;
        case 0x8D: CPU_8BIT_ADD(mRegisters.a, mRegisters.l, false, true);             return;
        case 0x8E: CPU_8BIT_ADD(mRegisters.a, readByte(mRegisters.hl), false, true);  return;
        case 0xCE: CPU_8BIT_ADD(mRegisters.a, 0x0, true, true);                       return;
        
        // 8-bit Subtraction
        case 0x97: CPU_8BIT_SUB(mRegisters.a, mRegisters.a, false, false);            return;
        case 0x90: CPU_8BIT_SUB(mRegisters.a, mRegisters.b, false, false);            return;
        case 0x91: CPU_8BIT_SUB(mRegisters.a, mRegisters.c, false, false);            return;
        case 0x92: CPU_8BIT_SUB(mRegisters.a, mRegisters.d, false, false);            return;
        case 0x93: CPU_8BIT_SUB(mRegisters.a, mRegisters.e, false, false);            return;
        case 0x94: CPU_8BIT_SUB(mRegisters.a, mRegisters.h, false, false);            return;
        case 0x95: CPU_8BIT_SUB(mRegisters.a, mRegisters.l, false, false);            return;
        case 0x96: CPU_8BIT_SUB(mRegisters.a, readByte(mRegisters.hl), false, false); return;
        case 0xD6: CPU_8BIT_SUB(mRegisters.a, 0x0, true, false);                      return;
        case 0x9F: CPU_8BIT_SUB(mRegisters.a, mRegisters.a, false, true);             return;
        case 0x98: CPU_8BIT_SUB(mRegisters.a, mRegisters.b, false, true);             return;
        case 0x99: CPU_8BIT_SUB(mRegisters.a, mRegisters.c, false, true);             return;
        case 0x9A: CPU_8BIT_SUB(mRegisters.a, mRegisters.d, false, true);             return;
        case 0x9B: CPU_8BIT_SUB(mRegisters.a, mRegisters.e, false, true);             return;
        case 0x9C: CPU_8BIT_SUB(mRegisters.a, mRegisters.h, false, true);             return;
        case 0x9D: CPU_8BIT_SUB(mRegisters.a, mRegisters.l, false, true);             return;
        case 0x9E: CPU_8BIT_SUB(mRegisters.a, readByte(mRegisters.hl), false, true);  return;
        case 0xDE: CPU_8BIT_SUB(mRegisters.a, 0x0, true, true);                       return;
        
        // 8-bit Logical AND
        case 0xA7: CPU_8BIT_AND(mRegisters.a, mRegisters.a, false);            return;
        case 0xA0: CPU_8BIT_AND(mRegisters.a, mRegisters.b, false);            return;
        case 0xA1: CPU_8BIT_AND(mRegisters.a, mRegisters.c, false);            return;
        case 0xA2: CPU_8BIT_AND(mRegisters.a, mRegisters.d, false);            return;
        case 0xA3: CPU_8BIT_AND(mRegisters.a, mRegisters.e, false);            return;
        case 0xA4: CPU_8BIT_AND(mRegisters.a, mRegisters.h, false);            return;
        case 0xA5: CPU_8BIT_AND(mRegisters.a, mRegisters.l, false);            return;
        case 0xA6: CPU_8BIT_AND(mRegisters.a, readByte(mRegisters.hl), false); return;
        case 0xE6: CPU_8BIT_AND(mRegisters.a, 0x0, true);                      return;
        
        // 8-bit Logical OR
        case 0xB7: CPU_8BIT_OR(mRegisters.a, mRegisters.a, false);            return;
        case 0xB0: CPU_8BIT_OR(mRegisters.a, mRegisters.b, false);            return;
        case 0xB1: CPU_8BIT_OR(mRegisters.a, mRegisters.c, false);            return;
        case 0xB2: CPU_8BIT_OR(mRegisters.a, mRegisters.d, false);            return;
        case 0xB3: CPU_8BIT_OR(mRegisters.a, mRegisters.e, false);            return;
        case 0xB4: CPU_8BIT_OR(mRegisters.a, mRegisters.h, false);            return;
        case 0xB5: CPU_8BIT_OR(mRegisters.a, mRegisters.l, false);            return;
        case 0xB6: CPU_8BIT_OR(mRegisters.a, readByte(mRegisters.hl), false); return;
        case 0xF6: CPU_8BIT_OR(mRegisters.a, 0x0, true);                      return;
        
        // 8-bit Logical XOR
        case 0xAF: CPU_8BIT_XOR(mRegisters.a, mRegisters.a, false);            return;
        case 0xA8: CPU_8BIT_XOR(mRegisters.a, mRegisters.b, false);            return;
        case 0xA9: CPU_8BIT_XOR(mRegisters.a, mRegisters.c, false);            return;
        case 0xAA: CPU_8BIT_XOR(mRegisters.a, mRegisters.d, false);            return;
        case 0xAB: CPU_8BIT_XOR(mRegisters.a, mRegisters.e, false);            return;
        case 0xAC: CPU_8BIT_XOR(mRegisters.a, mRegisters.h, false);            return;
        case 0xAD: CPU_8BIT_XOR(mRegisters.a, mRegisters.l, false);            return;
        case 0xAE: CPU_8BIT_XOR(mRegisters.a, readByte(mRegisters.hl), false); return;
        case 0xEE: CPU_8BIT_XOR(mRegisters.a, 0x0, true);                      return;
        
        // 8-bit Compare
        case 0xBF: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.a, false, false);            return;}
        case 0xB8: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.b, false, false);            return;}
        case 0xB9: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.c, false, false);            return;}
        case 0xBA: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.d, false, false);            return;}
        case 0xBB: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.e, false, false);            return;}
        case 0xBC: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.h, false, false);            return;}
        case 0xBD: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, mRegisters.l, false, false);            return;}
        case 0xBE: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, readByte(mRegisters.hl), false, false); return;}
        case 0xFE: {byte tmp = mRegisters.a; CPU_8BIT_SUB(tmp, 0x0, true, false);                      return;}
        
        // 8-bit Increment/Decrement
        case 0x3C: CPU_8BIT_INC(mRegisters.a);    return;
        case 0x04: CPU_8BIT_INC(mRegisters.b);    return;
        case 0x0C: CPU_8BIT_INC(mRegisters.c);    return;
        case 0x14: CPU_8BIT_INC(mRegisters.d);    return;
        case 0x1C: CPU_8BIT_INC(mRegisters.e);    return;
        case 0x24: CPU_8BIT_INC(mRegisters.h);    return;
        case 0x2C: CPU_8BIT_INC(mRegisters.l);    return;
        case 0x34: CPU_8BIT_INC(mRegisters.hl);   return;
        case 0x3D: CPU_8BIT_DEC(mRegisters.a);    return;
        case 0x05: CPU_8BIT_DEC(mRegisters.b);    return;
        case 0x0D: CPU_8BIT_DEC(mRegisters.c);    return;
        case 0x15: CPU_8BIT_DEC(mRegisters.d);    return;
        case 0x1D: CPU_8BIT_DEC(mRegisters.e);    return;
        case 0x25: CPU_8BIT_DEC(mRegisters.h);    return;
        case 0x2D: CPU_8BIT_DEC(mRegisters.l);    return;
        case 0x35: CPU_8BIT_DEC(mRegisters.hl);   return;
        
        // 16-bit Adddition
        case 0x09: CPU_16BIT_ADD(mRegisters.bc, mRegisters.hl);     return;
        case 0x19: CPU_16BIT_ADD(mRegisters.de, mRegisters.hl);     return;
        case 0x29: CPU_16BIT_ADD(mRegisters.hl, mRegisters.hl);     return;
        case 0x39: CPU_16BIT_ADD(mStackPointer.reg, mRegisters.hl); return;
        case 0xE8: CPU_16BIT_ADD(mStackPointer.reg);                return;
        
        // 16-bit Increment/Decrement
        case 0x03: mRegisters.bc++;     mEmulator->sync(4);          return;
        case 0x13: mRegisters.de++;     mEmulator->sync(4);          return;
        case 0x23: mRegisters.hl++;     mEmulator->sync(4);          return;
        case 0x33: mStackPointer.reg++; mEmulator->sync(4);          return;
        case 0x0B: mRegisters.bc--;     mEmulator->sync(4);          return;
        case 0x1B: mRegisters.de--;     mEmulator->sync(4);          return;
        case 0x2B: mRegisters.hl--;     mEmulator->sync(4);          return;
        case 0x3B: mStackPointer.reg--; mEmulator->sync(4);          return;
        
        // Misc
        case 0x27: CPU_DECIMAL_ADJUST();          return;
        case 0x2F: CPU_CPL();                     return;
        case 0x3F: CPU_CCF();                     return;
        case 0x37: CPU_SCF();                     return;
        case 0xF3: mPendingInterruptDisabled = 2; return;
        case 0xFB: mPendingInterruptEnabled  = 2; return;
        
        // Rotates/Shifts
        case 0x07: CPU_RLC(mRegisters.a); clearFlag(FLAG_ZERO); return; // RLCA != RLC A. RLCA always clears the zero flag.
        case 0x17: CPU_RL(mRegisters.a);  clearFlag(FLAG_ZERO); return; // Sim.
        case 0x0F: CPU_RRC(mRegisters.a); clearFlag(FLAG_ZERO); return;
        case 0x1F: CPU_RR(mRegisters.a);  clearFlag(FLAG_ZERO); return;
                
        // Jumps
        case 0xC3: CPU_JUMP(false, 0x0, false);                 return;
        case 0xC2: CPU_JUMP(true, FLAG_ZERO, false);            return;
        case 0xCA: CPU_JUMP(true, FLAG_ZERO, true);             return;
        case 0xD2: CPU_JUMP(true, FLAG_CARRY, false);           return;
        case 0xDA: CPU_JUMP(true, FLAG_CARRY, true);            return;
        case 0xE9: mProgramCounter = mRegisters.hl;             return;
        case 0x18: CPU_JUMP_IMMEDIATE(false, 0x0, false);       return;
        case 0x20: CPU_JUMP_IMMEDIATE(true, FLAG_ZERO, false);  return;
        case 0x28: CPU_JUMP_IMMEDIATE(true, FLAG_ZERO, true);   return;
        case 0x30: CPU_JUMP_IMMEDIATE(true, FLAG_CARRY, false); return;
        case 0x38: CPU_JUMP_IMMEDIATE(true, FLAG_CARRY, true);  return;
        
        // Calls
        case 0xCD: CPU_CALL(false, 0x0, false);       return;
        case 0xC4: CPU_CALL(true, FLAG_ZERO, false);  return;
        case 0xCC: CPU_CALL(true, FLAG_ZERO, true);   return;
        case 0xD4: CPU_CALL(true, FLAG_CARRY, false); return;
        case 0xDC: CPU_CALL(true, FLAG_CARRY, true);  return;
        
        // Restarts
        case 0xC7: CPU_RESTART(0x0000); return;
        case 0xCF: CPU_RESTART(0x0008); return;
        case 0xD7: CPU_RESTART(0x0010); return;
        case 0xDF: CPU_RESTART(0x0018); return;
        case 0xE7: CPU_RESTART(0x0020); return;
        case 0xEF: CPU_RESTART(0x0028); return;
        case 0xF7: CPU_RESTART(0x0030); return;
        case 0xFF: CPU_RESTART(0x0038); return;
        
        // Returns
        case 0xC9: CPU_RETURN();                          return;
        case 0xC0: CPU_RETURN_CC(!testFlag(FLAG_ZERO));   return;
        case 0xC8: CPU_RETURN_CC(testFlag(FLAG_ZERO));    return;
        case 0xD0: CPU_RETURN_CC(!testFlag(FLAG_CARRY));  return;
        case 0xD8: CPU_RETURN_CC(testFlag(FLAG_CARRY));   return;
        case 0xD9: mInterruptMaster = true; CPU_RETURN(); return;
        
        // Extended instructions
        case 0xCB: executeExtendedInstruction(); return;
        
        // Opcode not recognized
        default:
        {
            cerr << "Opcode not recognized: ";
            printHex(cerr, opcode);
            cerr << ", Program Counter: ";
            printHex(cerr, mProgramCounter - 1);
            cerr << endl;
            
            if (mDebugger != NULL) 
            {
                mDebugger->printStackTrace();
                mDebugger->printLastInstructions();
            }
        }
    }
}

void CPU::executeExtendedInstruction()
{
    byte opcode = getImmediateByte();
    
    switch (opcode)
    {
        // Rotate reg left
        case 0x00: CPU_RLC(mRegisters.b);  return;
        case 0x01: CPU_RLC(mRegisters.c);  return;
        case 0x02: CPU_RLC(mRegisters.d);  return;
        case 0x03: CPU_RLC(mRegisters.e);  return;
        case 0x04: CPU_RLC(mRegisters.h);  return;
        case 0x05: CPU_RLC(mRegisters.l);  return;
        case 0x06: {byte tmp = readByte(mRegisters.hl); CPU_RLC(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x07: CPU_RLC(mRegisters.a);  return;
        
        // Rotate reg right
        case 0x08: CPU_RRC(mRegisters.b);  return;
        case 0x09: CPU_RRC(mRegisters.c);  return;
        case 0x0A: CPU_RRC(mRegisters.d);  return;
        case 0x0B: CPU_RRC(mRegisters.e);  return;
        case 0x0C: CPU_RRC(mRegisters.h);  return;
        case 0x0D: CPU_RRC(mRegisters.l);  return;
        case 0x0E: {byte tmp = readByte(mRegisters.hl); CPU_RRC(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x0F: CPU_RRC(mRegisters.a);  return;
        
        // Rotate reg left through carry
        case 0x10: CPU_RL(mRegisters.b);  return;
        case 0x11: CPU_RL(mRegisters.c);  return;
        case 0x12: CPU_RL(mRegisters.d);  return;
        case 0x13: CPU_RL(mRegisters.e);  return;
        case 0x14: CPU_RL(mRegisters.h);  return;
        case 0x15: CPU_RL(mRegisters.l);  return;
        case 0x16: {byte tmp = readByte(mRegisters.hl); CPU_RL(tmp);  writeByte(mRegisters.hl, tmp); return;}
        case 0x17: CPU_RL(mRegisters.a);  return;
        
        // Rotate reg right through carry
        case 0x18: CPU_RR(mRegisters.b);  return;
        case 0x19: CPU_RR(mRegisters.c);  return;
        case 0x1A: CPU_RR(mRegisters.d);  return;
        case 0x1B: CPU_RR(mRegisters.e);  return;
        case 0x1C: CPU_RR(mRegisters.h);  return;
        case 0x1D: CPU_RR(mRegisters.l);  return;
        case 0x1E: {byte tmp = readByte(mRegisters.hl); CPU_RR(tmp);  writeByte(mRegisters.hl, tmp); return;}
        case 0x1F: CPU_RR(mRegisters.a);  return;
        
        // Shift left into carry
        case 0x20: CPU_SLA(mRegisters.b);  return;
        case 0x21: CPU_SLA(mRegisters.c);  return;
        case 0x22: CPU_SLA(mRegisters.d);  return;
        case 0x23: CPU_SLA(mRegisters.e);  return;
        case 0x24: CPU_SLA(mRegisters.h);  return;
        case 0x25: CPU_SLA(mRegisters.l);  return;
        case 0x26: {byte tmp = readByte(mRegisters.hl); CPU_SLA(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x27: CPU_SLA(mRegisters.a);  return;
        
        // Shift right into carry
        case 0x28: CPU_SRA(mRegisters.b);  return;
        case 0x29: CPU_SRA(mRegisters.c);  return;
        case 0x2A: CPU_SRA(mRegisters.d);  return;
        case 0x2B: CPU_SRA(mRegisters.e);  return;
        case 0x2C: CPU_SRA(mRegisters.h);  return;
        case 0x2D: CPU_SRA(mRegisters.l);  return;
        case 0x2E: {byte tmp = readByte(mRegisters.hl); CPU_SRA(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x2F: CPU_SRA(mRegisters.a);  return;
        
        // Swap
        case 0x30: CPU_SWAP(mRegisters.b); return;
        case 0x31: CPU_SWAP(mRegisters.c); return;
        case 0x32: CPU_SWAP(mRegisters.d); return;
        case 0x33: CPU_SWAP(mRegisters.e); return;
        case 0x34: CPU_SWAP(mRegisters.h); return;
        case 0x35: CPU_SWAP(mRegisters.l); return;
        case 0x36: {byte tmp = readByte(mRegisters.hl); CPU_SWAP(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x37: CPU_SWAP(mRegisters.a); return;

        // Shift right logically into carry
        case 0x38: CPU_SRL(mRegisters.b);  return;
        case 0x39: CPU_SRL(mRegisters.c);  return;
        case 0x3A: CPU_SRL(mRegisters.d);  return;
        case 0x3B: CPU_SRL(mRegisters.e);  return;
        case 0x3C: CPU_SRL(mRegisters.h);  return;
        case 0x3D: CPU_SRL(mRegisters.l);  return;
        case 0x3E: {byte tmp = readByte(mRegisters.hl); CPU_SRL(tmp); writeByte(mRegisters.hl, tmp); return;}
        case 0x3F: CPU_SRL(mRegisters.a);  return;
       
        // Test bit
        case 0x40: CPU_TEST_BIT(mRegisters.b, 0);            return;
        case 0x41: CPU_TEST_BIT(mRegisters.c, 0);            return;
        case 0x42: CPU_TEST_BIT(mRegisters.d, 0);            return;
        case 0x43: CPU_TEST_BIT(mRegisters.e, 0);            return;
        case 0x44: CPU_TEST_BIT(mRegisters.h, 0);            return;
        case 0x45: CPU_TEST_BIT(mRegisters.l, 0);            return;
        case 0x46: CPU_TEST_BIT(readByte(mRegisters.hl), 0); return;
        case 0x47: CPU_TEST_BIT(mRegisters.a, 0);            return;
        case 0x48: CPU_TEST_BIT(mRegisters.b, 1);            return;
        case 0x49: CPU_TEST_BIT(mRegisters.c, 1);            return;
        case 0x4A: CPU_TEST_BIT(mRegisters.d, 1);            return;
        case 0x4B: CPU_TEST_BIT(mRegisters.e, 1);            return;
        case 0x4C: CPU_TEST_BIT(mRegisters.h, 1);            return;
        case 0x4D: CPU_TEST_BIT(mRegisters.l, 1);            return;
        case 0x4E: CPU_TEST_BIT(readByte(mRegisters.hl), 1); return;
        case 0x4F: CPU_TEST_BIT(mRegisters.a, 1);            return;
        case 0x50: CPU_TEST_BIT(mRegisters.b, 2);            return;
        case 0x51: CPU_TEST_BIT(mRegisters.c, 2);            return;
        case 0x52: CPU_TEST_BIT(mRegisters.d, 2);            return;
        case 0x53: CPU_TEST_BIT(mRegisters.e, 2);            return;
        case 0x54: CPU_TEST_BIT(mRegisters.h, 2);            return;
        case 0x55: CPU_TEST_BIT(mRegisters.l, 2);            return;
        case 0x56: CPU_TEST_BIT(readByte(mRegisters.hl), 2); return;
        case 0x57: CPU_TEST_BIT(mRegisters.a, 2);            return;
        case 0x58: CPU_TEST_BIT(mRegisters.b, 3);            return;
        case 0x59: CPU_TEST_BIT(mRegisters.c, 3);            return;
        case 0x5A: CPU_TEST_BIT(mRegisters.d, 3);            return;
        case 0x5B: CPU_TEST_BIT(mRegisters.e, 3);            return;
        case 0x5C: CPU_TEST_BIT(mRegisters.h, 3);            return;
        case 0x5D: CPU_TEST_BIT(mRegisters.l, 3);            return;
        case 0x5E: CPU_TEST_BIT(readByte(mRegisters.hl), 3); return;
        case 0x5F: CPU_TEST_BIT(mRegisters.a, 3);            return;
        case 0x60: CPU_TEST_BIT(mRegisters.b, 4);            return;
        case 0x61: CPU_TEST_BIT(mRegisters.c, 4);            return;
        case 0x62: CPU_TEST_BIT(mRegisters.d, 4);            return;
        case 0x63: CPU_TEST_BIT(mRegisters.e, 4);            return;
        case 0x64: CPU_TEST_BIT(mRegisters.h, 4);            return;
        case 0x65: CPU_TEST_BIT(mRegisters.l, 4);            return;
        case 0x66: CPU_TEST_BIT(readByte(mRegisters.hl), 4); return;
        case 0x67: CPU_TEST_BIT(mRegisters.a, 4);            return;
        case 0x68: CPU_TEST_BIT(mRegisters.b, 5);            return;
        case 0x69: CPU_TEST_BIT(mRegisters.c, 5);            return;
        case 0x6A: CPU_TEST_BIT(mRegisters.d, 5);            return;
        case 0x6B: CPU_TEST_BIT(mRegisters.e, 5);            return;
        case 0x6C: CPU_TEST_BIT(mRegisters.h, 5);            return;
        case 0x6D: CPU_TEST_BIT(mRegisters.l, 5);            return;
        case 0x6E: CPU_TEST_BIT(readByte(mRegisters.hl), 5); return;
        case 0x6F: CPU_TEST_BIT(mRegisters.a, 5);            return;
        case 0x70: CPU_TEST_BIT(mRegisters.b, 6);            return;
        case 0x71: CPU_TEST_BIT(mRegisters.c, 6);            return;
        case 0x72: CPU_TEST_BIT(mRegisters.d, 6);            return;
        case 0x73: CPU_TEST_BIT(mRegisters.e, 6);            return;
        case 0x74: CPU_TEST_BIT(mRegisters.h, 6);            return;
        case 0x75: CPU_TEST_BIT(mRegisters.l, 6);            return;
        case 0x76: CPU_TEST_BIT(readByte(mRegisters.hl), 6); return;
        case 0x77: CPU_TEST_BIT(mRegisters.a, 6);            return;
        case 0x78: CPU_TEST_BIT(mRegisters.b, 7);            return;
        case 0x79: CPU_TEST_BIT(mRegisters.c, 7);            return;
        case 0x7A: CPU_TEST_BIT(mRegisters.d, 7);            return;
        case 0x7B: CPU_TEST_BIT(mRegisters.e, 7);            return;
        case 0x7C: CPU_TEST_BIT(mRegisters.h, 7);            return;
        case 0x7D: CPU_TEST_BIT(mRegisters.l, 7);            return;
        case 0x7E: CPU_TEST_BIT(readByte(mRegisters.hl), 7); return;
        case 0x7F: CPU_TEST_BIT(mRegisters.a, 7);            return;
        
        // Reset Bit
        case 0x80: mRegisters.b = clearBit(mRegisters.b, 0);            return;
        case 0x81: mRegisters.c = clearBit(mRegisters.c, 0);            return;
        case 0x82: mRegisters.d = clearBit(mRegisters.d, 0);            return;
        case 0x83: mRegisters.e = clearBit(mRegisters.e, 0);            return;
        case 0x84: mRegisters.h = clearBit(mRegisters.h, 0);            return;
        case 0x85: mRegisters.l = clearBit(mRegisters.l, 0);            return;
        case 0x86: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 0)); return;}
        case 0x87: mRegisters.a = clearBit(mRegisters.a, 0);            return;
        case 0x88: mRegisters.b = clearBit(mRegisters.b, 1);            return;
        case 0x89: mRegisters.c = clearBit(mRegisters.c, 1);            return;
        case 0x8A: mRegisters.d = clearBit(mRegisters.d, 1);            return;
        case 0x8B: mRegisters.e = clearBit(mRegisters.e, 1);            return;
        case 0x8C: mRegisters.h = clearBit(mRegisters.h, 1);            return;
        case 0x8D: mRegisters.l = clearBit(mRegisters.l, 1);            return;
        case 0x8E: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 1)); return;}
        case 0x8F: mRegisters.a = clearBit(mRegisters.a, 1);            return;
        case 0x90: mRegisters.b = clearBit(mRegisters.b, 2);            return;
        case 0x91: mRegisters.c = clearBit(mRegisters.c, 2);            return;
        case 0x92: mRegisters.d = clearBit(mRegisters.d, 2);            return;
        case 0x93: mRegisters.e = clearBit(mRegisters.e, 2);            return;
        case 0x94: mRegisters.h = clearBit(mRegisters.h, 2);            return;
        case 0x95: mRegisters.l = clearBit(mRegisters.l, 2);            return;
        case 0x96: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 2)); return;}
        case 0x97: mRegisters.a = clearBit(mRegisters.a, 2);            return;
        case 0x98: mRegisters.b = clearBit(mRegisters.b, 3);            return;
        case 0x99: mRegisters.c = clearBit(mRegisters.c, 3);            return;
        case 0x9A: mRegisters.d = clearBit(mRegisters.d, 3);            return;
        case 0x9B: mRegisters.e = clearBit(mRegisters.e, 3);            return;
        case 0x9C: mRegisters.h = clearBit(mRegisters.h, 3);            return;
        case 0x9D: mRegisters.l = clearBit(mRegisters.l, 3);            return;
        case 0x9E: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 3)); return;}
        case 0x9F: mRegisters.a = clearBit(mRegisters.a, 3);            return;
        case 0xA0: mRegisters.b = clearBit(mRegisters.b, 4);            return;
        case 0xA1: mRegisters.c = clearBit(mRegisters.c, 4);            return;
        case 0xA2: mRegisters.d = clearBit(mRegisters.d, 4);            return;
        case 0xA3: mRegisters.e = clearBit(mRegisters.e, 4);            return;
        case 0xA4: mRegisters.h = clearBit(mRegisters.h, 4);            return;
        case 0xA5: mRegisters.l = clearBit(mRegisters.l, 4);            return;
        case 0xA6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 4)); return;}
        case 0xA7: mRegisters.a = clearBit(mRegisters.a, 4);            return; 
        case 0xA8: mRegisters.b = clearBit(mRegisters.b, 5);            return;
        case 0xA9: mRegisters.c = clearBit(mRegisters.c, 5);            return;
        case 0xAA: mRegisters.d = clearBit(mRegisters.d, 5);            return;
        case 0xAB: mRegisters.e = clearBit(mRegisters.e, 5);            return;
        case 0xAC: mRegisters.h = clearBit(mRegisters.h, 5);            return;
        case 0xAD: mRegisters.l = clearBit(mRegisters.l, 5);            return;
        case 0xAE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 5)); return;}
        case 0xAF: mRegisters.a = clearBit(mRegisters.a, 5);            return;
        case 0xB0: mRegisters.b = clearBit(mRegisters.b, 6);            return;
        case 0xB1: mRegisters.c = clearBit(mRegisters.c, 6);            return;
        case 0xB2: mRegisters.d = clearBit(mRegisters.d, 6);            return;
        case 0xB3: mRegisters.e = clearBit(mRegisters.e, 6);            return;
        case 0xB4: mRegisters.h = clearBit(mRegisters.h, 6);            return;
        case 0xB5: mRegisters.l = clearBit(mRegisters.l, 6);            return;
        case 0xB6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 6)); return;}
        case 0xB7: mRegisters.a = clearBit(mRegisters.a, 6);            return;
        case 0xB8: mRegisters.b = clearBit(mRegisters.b, 7);            return;
        case 0xB9: mRegisters.c = clearBit(mRegisters.c, 7);            return;
        case 0xBA: mRegisters.d = clearBit(mRegisters.d, 7);            return;
        case 0xBB: mRegisters.e = clearBit(mRegisters.e, 7);            return;
        case 0xBC: mRegisters.h = clearBit(mRegisters.h, 7);            return;
        case 0xBD: mRegisters.l = clearBit(mRegisters.l, 7);            return;
        case 0xBE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, clearBit(n, 7)); return;}
        case 0xBF: mRegisters.a = clearBit(mRegisters.a, 7);            return;
        
        // Set Bit
        case 0xC0: mRegisters.b = setBit(mRegisters.b, 0);            return;
        case 0xC1: mRegisters.c = setBit(mRegisters.c, 0);            return;
        case 0xC2: mRegisters.d = setBit(mRegisters.d, 0);            return;
        case 0xC3: mRegisters.e = setBit(mRegisters.e, 0);            return;
        case 0xC4: mRegisters.h = setBit(mRegisters.h, 0);            return;
        case 0xC5: mRegisters.l = setBit(mRegisters.l, 0);            return;
        case 0xC6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 0)); return;}
        case 0xC7: mRegisters.a = setBit(mRegisters.a, 0);            return;
        case 0xC8: mRegisters.b = setBit(mRegisters.b, 1);            return;
        case 0xC9: mRegisters.c = setBit(mRegisters.c, 1);            return;
        case 0xCA: mRegisters.d = setBit(mRegisters.d, 1);            return;
        case 0xCB: mRegisters.e = setBit(mRegisters.e, 1);            return;
        case 0xCC: mRegisters.h = setBit(mRegisters.h, 1);            return;
        case 0xCD: mRegisters.l = setBit(mRegisters.l, 1);            return;
        case 0xCE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 1)); return;}
        case 0xCF: mRegisters.a = setBit(mRegisters.a, 1);            return;
        case 0xD0: mRegisters.b = setBit(mRegisters.b, 2);            return;
        case 0xD1: mRegisters.c = setBit(mRegisters.c, 2);            return;
        case 0xD2: mRegisters.d = setBit(mRegisters.d, 2);            return;
        case 0xD3: mRegisters.e = setBit(mRegisters.e, 2);            return;
        case 0xD4: mRegisters.h = setBit(mRegisters.h, 2);            return;
        case 0xD5: mRegisters.l = setBit(mRegisters.l, 2);            return;
        case 0xD6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 2)); return;}
        case 0xD7: mRegisters.a = setBit(mRegisters.a, 2);            return;
        case 0xD8: mRegisters.b = setBit(mRegisters.b, 3);            return;
        case 0xD9: mRegisters.c = setBit(mRegisters.c, 3);            return;
        case 0xDA: mRegisters.d = setBit(mRegisters.d, 3);            return;
        case 0xDB: mRegisters.e = setBit(mRegisters.e, 3);            return;
        case 0xDC: mRegisters.h = setBit(mRegisters.h, 3);            return;
        case 0xDD: mRegisters.l = setBit(mRegisters.l, 3);            return;
        case 0xDE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 3)); return;}
        case 0xDF: mRegisters.a = setBit(mRegisters.a, 3);            return;
        case 0xE0: mRegisters.b = setBit(mRegisters.b, 4);            return;
        case 0xE1: mRegisters.c = setBit(mRegisters.c, 4);            return;
        case 0xE2: mRegisters.d = setBit(mRegisters.d, 4);            return;
        case 0xE3: mRegisters.e = setBit(mRegisters.e, 4);            return;
        case 0xE4: mRegisters.h = setBit(mRegisters.h, 4);            return;
        case 0xE5: mRegisters.l = setBit(mRegisters.l, 4);            return;
        case 0xE6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 4)); return;}
        case 0xE7: mRegisters.a = setBit(mRegisters.a, 4);            return;
        case 0xE8: mRegisters.b = setBit(mRegisters.b, 5);            return;
        case 0xE9: mRegisters.c = setBit(mRegisters.c, 5);            return;
        case 0xEA: mRegisters.d = setBit(mRegisters.d, 5);            return;
        case 0xEB: mRegisters.e = setBit(mRegisters.e, 5);            return;
        case 0xEC: mRegisters.h = setBit(mRegisters.h, 5);            return;
        case 0xED: mRegisters.l = setBit(mRegisters.l, 5);            return;
        case 0xEE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 5)); return;}
        case 0xEF: mRegisters.a = setBit(mRegisters.a, 5);            return;
        case 0xF0: mRegisters.b = setBit(mRegisters.b, 6);            return;
        case 0xF1: mRegisters.c = setBit(mRegisters.c, 6);            return;
        case 0xF2: mRegisters.d = setBit(mRegisters.d, 6);            return;
        case 0xF3: mRegisters.e = setBit(mRegisters.e, 6);            return;
        case 0xF4: mRegisters.h = setBit(mRegisters.h, 6);            return;
        case 0xF5: mRegisters.l = setBit(mRegisters.l, 6);            return;
        case 0xF6: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 6)); return;}
        case 0xF7: mRegisters.a = setBit(mRegisters.a, 6);            return;  
        case 0xF8: mRegisters.b = setBit(mRegisters.b, 7);            return;
        case 0xF9: mRegisters.c = setBit(mRegisters.c, 7);            return;
        case 0xFA: mRegisters.d = setBit(mRegisters.d, 7);            return;
        case 0xFB: mRegisters.e = setBit(mRegisters.e, 7);            return;
        case 0xFC: mRegisters.h = setBit(mRegisters.h, 7);            return;
        case 0xFD: mRegisters.l = setBit(mRegisters.l, 7);            return;
        case 0xFE: {byte n = readByte(mRegisters.hl); writeByte(mRegisters.hl, setBit(n, 7)); return;}
        case 0xFF: mRegisters.a = setBit(mRegisters.a, 7);            return;
        
        default:
        {
            cerr << "Extended opcode not recognized: ";
            printHex(cerr, (0xCB << 8 | opcode));
            cerr << endl;
            
            if (mDebugger != NULL) mDebugger->printStackTrace();
        }
    }
}

// ---- BEGIN CPU INSTRUCTION IMPLEMENTATIONS ---- //

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
    
    mEmulator->sync(4);
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
    
    mEmulator->sync(4); 
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
    
    mEmulator->sync(8);
}

void CPU::CPU_JUMP_IMMEDIATE(bool useCondition, int flag, bool condition)
{
    signedByte n = (signedByte) getImmediateByte();
    if (!useCondition || (testFlag(flag) == condition))
    {
        mProgramCounter += n;
        mEmulator->sync(4);
    }
}

void CPU::CPU_JUMP(bool useCondition, int flag, bool condition)
{
    word address = getImmediateWord();
    if (!useCondition || (testFlag(flag) == condition))
    {
        mProgramCounter = address;
        mEmulator->sync(4);
    }
}

void CPU::CPU_CALL(bool useCondition, int flag, bool condition)
{
    word nn = getImmediateWord();
   
    if (!useCondition || (testFlag(flag) == condition))
    {
        if (mDebugger != NULL) mDebugger->CPUStackPush();
        pushWordOntoStack(mProgramCounter);
        mProgramCounter = nn;
    }
}

void CPU::CPU_RETURN_CC(bool condition)
{
    mEmulator->sync(4);
    if (condition)
    {
        if (mDebugger != NULL) mDebugger->CPUStackPop();
        mProgramCounter = popWordOffStack();
        mEmulator->sync(4);
    }
}

void CPU::CPU_RETURN()
{
    if (mDebugger != NULL) mDebugger->CPUStackPop();
    mProgramCounter = popWordOffStack();
    mEmulator->sync(4);
}

void CPU::CPU_RESTART(word addr)
{
    //if (mDebugger != NULL) mDebugger->CPUStackPush();
    pushWordOntoStack(mProgramCounter);
    mProgramCounter = addr;
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
        mHalt = true;
        
        // Check for the halt bug. Interrupts must be disabled and there must be at least one pending interrupt.
        if (!mInterruptMaster && (mMemory->read(INTERRUPT_ENABLED_REGISTER) & mMemory->read(INTERRUPT_REQUEST_REGISTER) & 0x1F))
        {
            // The halt bug was fixed for the GBC. All HALTS are followed by NOPs automatically.
            if (GBC(mConfig))
                mEmulator->sync(4);
            
            else mHaltBug = true;
        }
    }
}

void CPU::CPU_STOP()
{
    if (GBC(mConfig) && (mMemory->read(SPEED_SWITCH_ADDRESS) & 0x1))
    {
        mMemory->write(SPEED_SWITCH_ADDRESS, 0x0);
        mConfig->doubleSpeed = !mConfig->doubleSpeed;
    }
}

// ---- END CPU INSTRUCTION IMPLEMENTATIONS ---- //

string CPU::dissassembleInstruction(const word pc)
{
    byte opcode = mMemory->read(pc);
    
    // Convert the two arguments to strings (neither is guaranteed
    // to be used, though).
    char buffer[3];
    sprintf(buffer, "%02X", mMemory->read(pc + 1));
    string arg1(buffer);
    sprintf(buffer, "%02X", mMemory->read(pc + 2));
    string arg2(buffer);
    
    switch (opcode)
    {
        case 0x00: return "NOP";
        case 0x76: return "HALT";
        case 0x10: return "STOP";
        
        // Simple 8-bit loads
        case 0x06: return "LD B, " + arg1;
        case 0x0E: return "LD C, " + arg1;
        case 0x16: return "LD D, " + arg1;
        case 0x1E: return "LD E, " + arg1;
        case 0x26: return "LD H, " + arg1;
        case 0x2E: return "LD L, " + arg1;
        case 0x7F: return "LD A, A";
        case 0x78: return "LD A, B";
        case 0x79: return "LD A, C";
        case 0x7A: return "LD A, D";
        case 0x7B: return "LD A, E";
        case 0x7C: return "LD A, H";
        case 0x7D: return "LD A, L";
        case 0x7E: return "LD A, (HL)";
        case 0x47: return "LD B, A";
        case 0x40: return "LD B, B";
        case 0x41: return "LD B, C";
        case 0x42: return "LD B, D";
        case 0x43: return "LD B, E";
        case 0x44: return "LD B, H";
        case 0x45: return "LD B, L";
        case 0x46: return "LD B, (HL)";
        case 0x4F: return "LD C, A";
        case 0x48: return "LD C, B";
        case 0x49: return "LD C, C";
        case 0x4A: return "LD C, D";
        case 0x4B: return "LD C, E";
        case 0x4C: return "LD C, H";
        case 0x4D: return "LD C, L";
        case 0x4E: return "LD C, (HL)";
        case 0x57: return "LD D, A";
        case 0x50: return "LD D, B";
        case 0x51: return "LD D, C";
        case 0x52: return "LD D, D";
        case 0x53: return "LD D, E";
        case 0x54: return "LD D, H";
        case 0x55: return "LD D, L";
        case 0x56: return "LD D, (HL)";
        case 0x5F: return "LD E, A";
        case 0x58: return "LD E, B";
        case 0x59: return "LD E, C";
        case 0x5A: return "LD E, D";
        case 0x5B: return "LD E, E";
        case 0x5C: return "LD E, H";
        case 0x5D: return "LD E, L";
        case 0x5E: return "LD E, (HL)";
        case 0x67: return "LD H, A";
        case 0x60: return "LD H, B";
        case 0x61: return "LD H, C";
        case 0x62: return "LD H, D";
        case 0x63: return "LD H, E";
        case 0x64: return "LD H, H";
        case 0x65: return "LD H, L";
        case 0x66: return "LD H, (HL)";
        case 0x6F: return "LD L, A";
        case 0x68: return "LD L, B";
        case 0x69: return "LD L, C";
        case 0x6A: return "LD L, D";
        case 0x6B: return "LD L, E";
        case 0x6C: return "LD L, H";
        case 0x6D: return "LD L, L";
        case 0x6E: return "LD L, (HL)";
        case 0x70: return "LD (HL), B";
        case 0x71: return "LD (HL), C";
        case 0x72: return "LD (HL), D";
        case 0x73: return "LD (HL), E";
        case 0x74: return "LD (HL), H";
        case 0x75: return "LD (HL), L";
        case 0x36: return "LD (HL), " + arg1;
        case 0x0A: return "LD A, (BC)";
        case 0x1A: return "LD A, (DE)";
        case 0xFA: return "LD A, (" + arg2 + arg1 + ")";
        case 0x3E: return "LD A, " + arg1;
        case 0x02: return "LD (BC), A";
        case 0x12: return "LD (DE), A";
        case 0x77: return "LD (HL), A";
        case 0xEA: return "LD (" + arg2 + arg1 + "), A";
        
        // Load Increment/Decrement
        case 0xF2: return "LD A, (FF00+C)";
        case 0xE2: return "LD (FF00+C), A";
        case 0x3A: return "LDD A, (HL)";
        case 0x32: return "LDD (HL), A";
        case 0x2A: return "LDI A, (HL)";
        case 0x22: return "LDI (HL), A";
        case 0xE0: return "LD (FF00+" + arg1 + "), A";
        case 0xF0: return "LD A, (FF00+" + arg1 +")";
        
        // 16 bit loads
        case 0x01: return "LD BC, " + arg2 + arg1;
        case 0x11: return "LD DE, " + arg2 + arg1;
        case 0x21: return "LD HL, " + arg2 + arg1;
        case 0x31: return "LD SP, " + arg2 + arg1;
        case 0xF9: return "LD SP, HL";
        case 0xF8: return "LD HL, SP+" + arg1;
        case 0x08: return "LD (" + arg2 + arg1 + "), SP";
        
        // Stack Operations
        case 0xF5: return "PUSH AF";
        case 0xC5: return "PUSH BC";
        case 0xD5: return "PUSH DE";
        case 0xE5: return "PUSH HL";
        case 0xF1: return "POP AF";
        case 0xC1: return "POP BC";
        case 0xD1: return "POP DE";
        case 0xE1: return "POP HL";
        
        // 8-bit Addition / Subtraction
        case 0x87: return "ADD A, A";
        case 0x80: return "ADD A, B";
        case 0x81: return "ADD A, C";
        case 0x82: return "ADD A, D";
        case 0x83: return "ADD A, E";
        case 0x84: return "ADD A, H";
        case 0x85: return "ADD A, L";
        case 0x86: return "ADD A, (HL)";
        case 0xC6: return "ADD A, " + arg1;
        case 0x8F: return "ADC A, A";
        case 0x88: return "ADC A, B";
        case 0x89: return "ADC A, C";
        case 0x8A: return "ADC A, D";
        case 0x8B: return "ADC A, E";
        case 0x8C: return "ADC A, H";
        case 0x8D: return "ADC A, L";
        case 0x8E: return "ADC A, (HL)";
        case 0xCE: return "ADC A, " + arg1;
        case 0x97: return "SUB A";
        case 0x90: return "SUB B";
        case 0x91: return "SUB C";
        case 0x92: return "SUB D";
        case 0x93: return "SUB E";
        case 0x94: return "SUB H";
        case 0x95: return "SUB L";
        case 0x96: return "SUB (HL)";
        case 0xD6: return "SUB A, " + arg1;
        case 0x9F: return "SBC A, A";
        case 0x98: return "SBC A, B";
        case 0x99: return "SBC A, C";
        case 0x9A: return "SBC A, D";
        case 0x9B: return "SBC A, E";
        case 0x9C: return "SBC A, H";
        case 0x9D: return "SBC A, L";
        case 0x9E: return "SBC A, (HL)";
        case 0xDE: return "SBC A, " + arg1;
        
        // 8-bit Logical AND
        case 0xA7: return "AND A";
        case 0xA0: return "AND B";
        case 0xA1: return "AND C";
        case 0xA2: return "AND D";
        case 0xA3: return "AND E";
        case 0xA4: return "AND H";
        case 0xA5: return "AND L";
        case 0xA6: return "AND (HL)";
        case 0xE6: return "AND A, " + arg1;
        
        // 8-bit Logical OR
        case 0xB7: return "OR A";
        case 0xB0: return "OR B";
        case 0xB1: return "OR C";
        case 0xB2: return "OR D";
        case 0xB3: return "OR E";
        case 0xB4: return "OR H";
        case 0xB5: return "OR L";
        case 0xB6: return "OR (HL)";
        case 0xF6: return "OR A, " + arg1;
        
        // 8-bit Logical XOR
        case 0xAF: return "XOR A";
        case 0xA8: return "XOR B";
        case 0xA9: return "XOR C";
        case 0xAA: return "XOR D";
        case 0xAB: return "XOR E";
        case 0xAC: return "XOR H";
        case 0xAD: return "XOR L";
        case 0xAE: return "XOR (HL)";
        case 0xEE: return "XOR A, " + arg1;
        
        // 8-bit Compare
        case 0xBF: return "CP A";
        case 0xB8: return "CP B";
        case 0xB9: return "CP C";
        case 0xBA: return "CP D";
        case 0xBB: return "CP E";
        case 0xBC: return "CP H";
        case 0xBD: return "CP L";
        case 0xBE: return "CP (HL)";
        case 0xFE: return "CP A, " + arg1;
        
        // 8-bit Increment/Decrement
        case 0x3C: return "INC A";
        case 0x04: return "INC B";
        case 0x0C: return "INC C";
        case 0x14: return "INC D";
        case 0x1C: return "INC E";
        case 0x24: return "INC H";
        case 0x2C: return "INC L";
        case 0x34: return "INC (HL)";
        case 0x3D: return "DEC A";
        case 0x05: return "DEC B";
        case 0x0D: return "DEC C";
        case 0x15: return "DEC D";
        case 0x1D: return "DEC E";
        case 0x25: return "DEC H";
        case 0x2D: return "DEC L";
        case 0x35: return "DEC (HL)";
        
        // 16-bit Adddition
        case 0x09: return "ADD HL, BC";
        case 0x19: return "ADD HL, DE";
        case 0x29: return "ADD HL, HL";
        case 0x39: return "ADD HL, SP";
        case 0xE8: return "ADD SP, " + arg1;
        
        // 16-bit Increment/Decrement
        case 0x03: return "INC BC";
        case 0x13: return "INC DE";
        case 0x23: return "INC HL";
        case 0x33: return "INC SP";
        case 0x0B: return "DEC BC";
        case 0x1B: return "DEC DE";
        case 0x2B: return "DEC HL";
        case 0x3B: return "DEC SP";
        
        // Misc
        case 0x27: return "DAA";
        case 0x2F: return "CPL";
        case 0x3F: return "CCF";
        case 0x37: return "SCF";
        case 0xF3: return "DI";
        case 0xFB: return "EI";
        
        // Rotates/Shifts
        case 0x07: return "RLCA";
        case 0x17: return "RLA";
        case 0x0F: return "RRCA";
        case 0x1F: return "RRA";
                
        // Jumps
        case 0xC3: return "JP "    + arg2 + arg1;
        case 0xC2: return "JP NZ " + arg2 + arg1;
        case 0xCA: return "JP Z "  + arg2 + arg1;
        case 0xD2: return "JP NC " + arg2 + arg1;
        case 0xDA: return "JP C "  + arg2 + arg1;
        case 0xE9: return "JP (HL)";
        case 0x18: 
        {
            char b[5];
            sprintf(b, "%04X", pc + 2 + ((signedByte) mMemory->read(pc + 1)));
            return "JR " + string(b);
        }
        case 0x20:
        {
            char b[5];
            sprintf(b, "%04X", pc + 2 + ((signedByte) mMemory->read(pc + 1)));
            return "JR NZ, " + string(b);
        }
        case 0x28: 
        {
            char b[5];
            sprintf(b, "%04X", pc + 2 + ((signedByte) mMemory->read(pc + 1)));
            return "JR Z, " + string(b);
        }
        case 0x30:
        {
            char b[5];
            sprintf(b, "%04X", pc + 2 + ((signedByte) mMemory->read(pc + 1)));
            return "JR NC, " + string(b);
        }
        case 0x38: 
        {
            char b[5];
            sprintf(b, "%04X", pc + 2 + ((signedByte) mMemory->read(pc + 1)));
            return "JR C, " + string(b);
        }
        
        // Calls
        case 0xCD: return "CALL "     + arg2 + arg1;
        case 0xC4: return "CALL NZ, " + arg2 + arg1;
        case 0xCC: return "CALL Z, "  + arg2 + arg1;
        case 0xD4: return "CALL NC, " + arg2 + arg1;
        case 0xDC: return "CALL C, "  + arg2 + arg1;
        
        // Restarts
        case 0xC7: return "RST 00";
        case 0xCF: return "RST 08";
        case 0xD7: return "RST 10";
        case 0xDF: return "RST 18";
        case 0xE7: return "RST 20";
        case 0xEF: return "RST 28";
        case 0xF7: return "RST 30";
        case 0xFF: return "RST 38";
        
        // Returns
        case 0xC9: return "RET";
        case 0xC0: return "RET NZ";
        case 0xC8: return "RET Z";
        case 0xD0: return "RET NC";
        case 0xD8: return "RET C";
        case 0xD9: return "RETI";
        
        // Extended opcodes
        case 0xCB:
        {
            switch (mMemory->read(pc + 1))
            {
                // Rotate reg left
                case 0x00: return "RLC B";
                case 0x01: return "RLC C";
                case 0x02: return "RLC D";
                case 0x03: return "RLC E";
                case 0x04: return "RLC H";
                case 0x05: return "RLC L";
                case 0x06: return "RLC (HL)";
                case 0x07: return "RLC A";

                // Rotate reg right
                case 0x08: return "RRC B";
                case 0x09: return "RRC C";
                case 0x0A: return "RRC D";
                case 0x0B: return "RRC E";
                case 0x0C: return "RRC H";
                case 0x0D: return "RRC L";
                case 0x0E: return "RRC (HL)";
                case 0x0F: return "RRC A";

                // Rotate reg left through carry
                case 0x10: return "RL B";
                case 0x11: return "RL C";
                case 0x12: return "RL D";
                case 0x13: return "RL E";
                case 0x14: return "RL H";
                case 0x15: return "RL L";
                case 0x16: return "RL (HL)";
                case 0x17: return "RL A";

                // Rotate reg right through carry
                case 0x18: return "RR B";
                case 0x19: return "RR C";
                case 0x1A: return "RR D";
                case 0x1B: return "RR E";
                case 0x1C: return "RR H";
                case 0x1D: return "RR L";
                case 0x1E: return "RR (HL)";
                case 0x1F: return "RR A";

                // Shift left into carry
                case 0x20: return "SLA B";
                case 0x21: return "SLA C";
                case 0x22: return "SLA D";
                case 0x23: return "SLA E";
                case 0x24: return "SLA H";
                case 0x25: return "SLA L";
                case 0x26: return "SLA (HL)";
                case 0x27: return "SLA A";

                // Shift right into carry
                case 0x28: return "SRA B";
                case 0x29: return "SRA C";
                case 0x2A: return "SRA D";
                case 0x2B: return "SRA E";
                case 0x2C: return "SRA H";
                case 0x2D: return "SRA L";
                case 0x2E: return "SRA (HL)";
                case 0x2F: return "SRA A";

                // Swap
                case 0x30: return "SWAP B";
                case 0x31: return "SWAP C";
                case 0x32: return "SWAP D";
                case 0x33: return "SWAP E";
                case 0x34: return "SWAP H";
                case 0x35: return "SWAP L";
                case 0x36: return "SWAP (HL)";
                case 0x37: return "SWAP A";

                // Shift right logically into carry
                case 0x38: return "SRL B";
                case 0x39: return "SRL C";
                case 0x3A: return "SRL D";
                case 0x3B: return "SRL E";
                case 0x3C: return "SRL H";
                case 0x3D: return "SRL L";
                case 0x3E: return "SRL (HL)";
                case 0x3F: return "SRL A";

                // Test bit
                case 0x40: return "BIT 0, B";
                case 0x41: return "BIT 0, C";
                case 0x42: return "BIT 0, D";
                case 0x43: return "BIT 0, E";
                case 0x44: return "BIT 0, H";
                case 0x45: return "BIT 0, L";
                case 0x46: return "BIT 0, (HL)";
                case 0x47: return "BIT 0, A";
                case 0x48: return "BIT 1, B";
                case 0x49: return "BIT 1, C";
                case 0x4A: return "BIT 1, D";
                case 0x4B: return "BIT 1, E";
                case 0x4C: return "BIT 1, H";
                case 0x4D: return "BIT 1, L";
                case 0x4E: return "BIT 1, (HL)";
                case 0x4F: return "BIT 1, A";
                case 0x50: return "BIT 2, B";
                case 0x51: return "BIT 2, C";
                case 0x52: return "BIT 2, D";
                case 0x53: return "BIT 2, E";
                case 0x54: return "BIT 2, H";
                case 0x55: return "BIT 2, L";
                case 0x56: return "BIT 2, (HL)";
                case 0x57: return "BIT 2, A";
                case 0x58: return "BIT 3, B";
                case 0x59: return "BIT 3, C";
                case 0x5A: return "BIT 3, D";
                case 0x5B: return "BIT 3, E";
                case 0x5C: return "BIT 3, H";
                case 0x5D: return "BIT 3, L";
                case 0x5E: return "BIT 3, (HL)";
                case 0x5F: return "BIT 3, A";
                case 0x60: return "BIT 4, B";
                case 0x61: return "BIT 4, C";
                case 0x62: return "BIT 4, D";
                case 0x63: return "BIT 4, E";
                case 0x64: return "BIT 4, H";
                case 0x65: return "BIT 4, L";
                case 0x66: return "BIT 4, (HL)";
                case 0x67: return "BIT 4, A";
                case 0x68: return "BIT 5, B";
                case 0x69: return "BIT 5, C";
                case 0x6A: return "BIT 5, D";
                case 0x6B: return "BIT 5, E";
                case 0x6C: return "BIT 5, H";
                case 0x6D: return "BIT 5, L";
                case 0x6E: return "BIT 5, (HL)";
                case 0x6F: return "BIT 5, A";
                case 0x70: return "BIT 6, B";
                case 0x71: return "BIT 6, C";
                case 0x72: return "BIT 6, D";
                case 0x73: return "BIT 6, E";
                case 0x74: return "BIT 6, H";
                case 0x75: return "BIT 6, L";
                case 0x76: return "BIT 6, (HL)";
                case 0x77: return "BIT 6, A";
                case 0x78: return "BIT 7, B";
                case 0x79: return "BIT 7, C";
                case 0x7A: return "BIT 7, D";
                case 0x7B: return "BIT 7, E";
                case 0x7C: return "BIT 7, H";
                case 0x7D: return "BIT 7, L";
                case 0x7E: return "BIT 7, (HL)";
                case 0x7F: return "BIT 7, A";

                // Reset Bit
                case 0x80: return "RES 0, B";
                case 0x81: return "RES 0, C";
                case 0x82: return "RES 0, D";
                case 0x83: return "RES 0, E";
                case 0x84: return "RES 0, H";
                case 0x85: return "RES 0, L";
                case 0x86: return "RES 0, (HL)";
                case 0x87: return "RES 0, A";
                case 0x88: return "RES 1, B";
                case 0x89: return "RES 1, C";
                case 0x8A: return "RES 1, D";
                case 0x8B: return "RES 1, E";
                case 0x8C: return "RES 1, H";
                case 0x8D: return "RES 1, L";
                case 0x8E: return "RES 1, (HL)";
                case 0x8F: return "RES 1, A";
                case 0x90: return "RES 2, B";
                case 0x91: return "RES 2, C";
                case 0x92: return "RES 2, D";
                case 0x93: return "RES 2, E";
                case 0x94: return "RES 2, H";
                case 0x95: return "RES 2, L";
                case 0x96: return "RES 2, (HL)";
                case 0x97: return "RES 2, A";
                case 0x98: return "RES 3, B";
                case 0x99: return "RES 3, C";
                case 0x9A: return "RES 3, D";
                case 0x9B: return "RES 3, E";
                case 0x9C: return "RES 3, H";
                case 0x9D: return "RES 3, L";
                case 0x9E: return "RES 3, (HL)";
                case 0x9F: return "RES 3, A";
                case 0xA0: return "RES 4, B";
                case 0xA1: return "RES 4, C";
                case 0xA2: return "RES 4, D";
                case 0xA3: return "RES 4, E";
                case 0xA4: return "RES 4, H";
                case 0xA5: return "RES 4, L";
                case 0xA6: return "RES 4, (HL)";
                case 0xA7: return "RES 4, A";
                case 0xA8: return "RES 5, B";
                case 0xA9: return "RES 5, C";
                case 0xAA: return "RES 5, D";
                case 0xAB: return "RES 5, E";
                case 0xAC: return "RES 5, H";
                case 0xAD: return "RES 5, L";
                case 0xAE: return "RES 5, (HL)";
                case 0xAF: return "RES 5, A";
                case 0xB0: return "RES 6, B";
                case 0xB1: return "RES 6, C";
                case 0xB2: return "RES 6, D";
                case 0xB3: return "RES 6, E";
                case 0xB4: return "RES 6, H";
                case 0xB5: return "RES 6, L";
                case 0xB6: return "RES 6, (HL)";
                case 0xB7: return "RES 6, A";
                case 0xB8: return "RES 7, B";
                case 0xB9: return "RES 7, C";
                case 0xBA: return "RES 7, D";
                case 0xBB: return "RES 7, E";
                case 0xBC: return "RES 7, H";
                case 0xBD: return "RES 7, L";
                case 0xBE: return "RES 7, (HL)";
                case 0xBF: return "RES 7, A";

                // Set Bit - [C0 - FF]
                case 0xC0: return "SET 0, B";
                case 0xC1: return "SET 0, C";
                case 0xC2: return "SET 0, D";
                case 0xC3: return "SET 0, E";
                case 0xC4: return "SET 0, H";
                case 0xC5: return "SET 0, L";
                case 0xC6: return "SET 0, (HL)";
                case 0xC7: return "SET 0, A";
                case 0xC8: return "SET 1, B";
                case 0xC9: return "SET 1, C";
                case 0xCA: return "SET 1, D";
                case 0xCB: return "SET 1, E";
                case 0xCC: return "SET 1, H";
                case 0xCD: return "SET 1, L";
                case 0xCE: return "SET 1, (HL)";
                case 0xCF: return "SET 1, A";
                case 0xD0: return "SET 2, B";
                case 0xD1: return "SET 2, C";
                case 0xD2: return "SET 2, D";
                case 0xD3: return "SET 2, E";
                case 0xD4: return "SET 2, H";
                case 0xD5: return "SET 2, L";
                case 0xD6: return "SET 2, (HL)";
                case 0xD7: return "SET 2, A";
                case 0xD8: return "SET 3, B";
                case 0xD9: return "SET 3, C";
                case 0xDA: return "SET 3, D";
                case 0xDB: return "SET 3, E";
                case 0xDC: return "SET 3, H";
                case 0xDD: return "SET 3, L";
                case 0xDE: return "SET 3, (HL)";
                case 0xDF: return "SET 3, A";
                case 0xE0: return "SET 4, B";
                case 0xE1: return "SET 4, C";
                case 0xE2: return "SET 4, D";
                case 0xE3: return "SET 4, E";
                case 0xE4: return "SET 4, H";
                case 0xE5: return "SET 4, L";
                case 0xE6: return "SET 4, (HL)";
                case 0xE7: return "SET 4, A";
                case 0xE8: return "SET 5, B";
                case 0xE9: return "SET 5, C";
                case 0xEA: return "SET 5, D";
                case 0xEB: return "SET 5, E";
                case 0xEC: return "SET 5, H";
                case 0xED: return "SET 5, L";
                case 0xEE: return "SET 5, (HL)";
                case 0xEF: return "SET 5, A";
                case 0xF0: return "SET 6, B";
                case 0xF1: return "SET 6, C";
                case 0xF2: return "SET 6, D";
                case 0xF3: return "SET 6, E";
                case 0xF4: return "SET 6, H";
                case 0xF5: return "SET 6, L";
                case 0xF6: return "SET 6, (HL)";
                case 0xF7: return "SET 6, A";
                case 0xF8: return "SET 7, B";
                case 0xF9: return "SET 7, C";
                case 0xFA: return "SET 7, D";
                case 0xFB: return "SET 7, E";
                case 0xFC: return "SET 7, H";
                case 0xFD: return "SET 7, L";
                case 0xFE: return "SET 7, (HL)";
                case 0xFF: return "SET 7, A";

                default: return "[Extended opcode not recognized]";
            }
        }
        
        default:
        {
            return "[Opcode not recognized]";
        }
    }
}