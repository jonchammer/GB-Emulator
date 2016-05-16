/* 
 * File:   Emulator.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 9:49 AM
 */

#include "Emulator.h"

Emulator::Emulator(bool skipBIOS) : mPaused(false)
{
    // Create the Memory and CPU
    mMemory   = new Memory(this, skipBIOS);
    mCPU      = new CPU(this, mMemory, skipBIOS); 
    mGraphics = new Graphics(mMemory);
    mInput    = new Input(mMemory);
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
    // Respect the user's desire to pause emulation
    if (mPaused) return;
    
//    const int MAX_CYCLES = 69905; // Or 70221?
//    int cyclesThisUpdate = 0;
//    
//    while (cyclesThisUpdate < MAX_CYCLES)
//    {
//        int cycles = mCPU->executeNextOpcode();
//        cyclesThisUpdate += cycles;
//        
//        mMemory->updateTimers(cycles);
//        mGraphics->update(cycles);
//        mSound->update(cycles);
//        
//        cyclesThisUpdate += mCPU->handleInterrupts();
//    }
    
    // The gameboy can execute 4194304 cycles in one second. At 60 fps,
    // we need to execute exactly 69905 cycles per frame.
    const int MAX_CYCLES = 69905;
    
    mCycleCount = 0;
    
    while (mCycleCount < MAX_CYCLES)
    {
        mCPU->executeNextOpcode();
    }
}

void Emulator::sync(int cycles)
{
    mGraphics->update(cycles);
    mMemory->updateTimers(cycles); 
    mSound->update(cycles);
    
    mCycleCount += cycles;
}