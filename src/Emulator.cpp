/* 
 * File:   Emulator.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 9:49 AM
 */

#include "Emulator.h"

Emulator::Emulator(bool skipBIOS)
{
    // Create the Memory and CPU
    mMemory   = new Memory(this, skipBIOS);
    mCPU      = new CPU(mMemory, skipBIOS); 
    mGraphics = new Graphics(mMemory);
    mInput    = new Input(mMemory, this);
    mSound    = new Sound(mMemory, skipBIOS, 44100, 1024);
}

Emulator::~Emulator()
{
    delete mCPU;
    delete mMemory;
    delete mGraphics;
    delete mInput;
    delete mSound;
}

void Emulator::reset(bool skipBIOS)
{
    mMemory->reset(skipBIOS);
    mCPU->reset(skipBIOS);
    mGraphics->reset();
    mInput->reset();
    mSound->reset(skipBIOS);
}

// Should be called 60x / second
void Emulator::update()
{
    const int MAX_CYCLES = 69905; // Or 70221?
    int cyclesThisUpdate = 0;
    
    while (cyclesThisUpdate < MAX_CYCLES)
    {
        int cycles = mCPU->executeNextOpcode();
        cyclesThisUpdate += cycles;
        
        mMemory->updateTimers(cycles);
        mGraphics->update(this, cycles);
        mSound->update(cycles);
        
        cyclesThisUpdate += mCPU->handleInterrupts();
    }
}

void Emulator::requestInterrupt(int id)
{
    byte req = mMemory->read(INTERRUPT_REQUEST_REGISTER);
    req      = setBit(req, id);
    mMemory->write(INTERRUPT_REQUEST_REGISTER, req);
}