/* 
 * File:   Emulator.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 9:49 AM
 */

#include "Emulator.h"
#include "GBGraphics.h"
#include "GBCGraphics.h"

Emulator::Emulator(EmulatorConfiguration* configuration) : mConfig(configuration), mClassOwnsConfig(false), mPaused(false)
{
    if (mConfig == NULL)
    {
        cout << "No configuration given. Using default instead." << endl;
        mConfig          = new EmulatorConfiguration();
        mClassOwnsConfig = true;
    }
    
    // Create the various components we need
    mMemory   = new Memory(this, mConfig);
    mCPU      = new CPU(this, mMemory, mConfig); 
    mTimers   = new Timers(mMemory, mConfig);
    mInput    = new Input(mMemory, mConfig);
    mSound    = new Sound(mMemory, mConfig);
    mGraphics = NULL;
}

Emulator::~Emulator()
{
    delete mCPU;
    delete mMemory;
    delete mTimers;
    delete mGraphics;
    delete mInput;
    delete mSound;
    
    // If we created a configuration, we need to be the ones to clean it up.
    if (mClassOwnsConfig)
    {
        delete mConfig;
        mConfig = NULL;
    }
}

void Emulator::reset()
{
    mMemory->reset();
    mCPU->reset();
    mTimers->reset();
    mGraphics->reset();
    mInput->reset();
    mSound->reset();
}

// Should be called 60x / second
void Emulator::update()
{
    // Respect the user's desire to pause emulation
    if (mPaused) return;
    
    // The gameboy can execute 4194304 cycles in one second. At ~59.73 fps,
    // we need to execute 70224 cycles per frame.
    const int MAX_CYCLES = 70224;
    
    // The cycle count is updated by the 'sync' function, which is called by
    // the CPU one or more times during an update step.
    mCycleCount = 0;
    while (mCycleCount < MAX_CYCLES)
        mCPU->update();
}

void Emulator::attachDebugger(Debugger* debugger)
{
    mCPU->attachDebugger(debugger);
    mMemory->attachDebugger(debugger);
    mInput->attachDebugger(debugger);
    
    debugger->attachCPU(mCPU);
    debugger->attachMemory(mMemory);
    debugger->attachInput(mInput);
}

void Emulator::loadCartridge(Cartridge* cartridge)
{
    mMemory->loadCartridge(cartridge);
    
    // Set the system correctly. AUTOMATIC means we will use
    // a gameboy if the cartridge is a gameboy cartridge and
    // a gameboy color if the cartridge is a gameboy color cartridge.
    if (mConfig->system == System::AUTOMATIC)
    {
        if (cartridge->getCartridgeInfo().gbc)
        {
            mConfig->system = System::GBC;
            mGraphics = new GBCGraphics(mMemory, mConfig);
        }
        else 
        {
            mConfig->system = System::GB;
            mGraphics = new GBGraphics(mMemory, mConfig);
        }
        
        // Some components need to be made aware that there has been
        // a configuration change
        reset();
    }
}

void Emulator::sync(int cycles)
{
    if (GBC(mConfig) && mConfig->doubleSpeed)
        cycles >>= 1;
    
    mGraphics->update(cycles);
    mTimers->update(cycles);
    mSound->update(cycles);
    
    mCycleCount += cycles;
}