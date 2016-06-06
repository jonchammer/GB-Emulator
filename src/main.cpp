/* 
 * File:   main.cpp
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 8:43 AM
 */

#include <cstdlib>
#include <iostream>

#include "Memory.h"
#include "Emulator.h"
#include "Cartridge.h"
#include "Debugger.h"
using namespace std;

int main(int argc, char** argv)
{
    //string game = "../roms/tests/MemoryTiming/mem_timing.gb";
    string game = "../roms/Pokemon_Crystal.gbc";
    //string game = "../roms/Tetris.gb";
    
    // Create the system configuration that will be used
    EmulatorConfiguration config;
    config.skipBIOS = true;
    config.system   = System::GBC;
    config.palette  = GameboyPalette::REAL;
    
    // Create the emulator
    Emulator emulator(&config);
    
    // Create & load the game cartridge
    Cartridge cartridge;
    if (!cartridge.load(game))
    {
        cerr << "Unable to load game: " << game << endl;
        return 1;
    }
    emulator.loadCartridge(&cartridge);
    cartridge.printInfo();
  
    // Create and configure debugger
    //Debugger debugger;
    //debugger.setEnabled(true);
    //debugger.setNumLastInstructions(200);
    //debugger.setBreakpoint(0x6796);
    //debugger.setBreakpoint(0x67AA);
    //debugger.setPaused(true);
    //debugger.setJoypadBreakpoint(BUTTON_START);
    //debugger.setBreakpoint(0x28AA);
    //emulator.attachDebugger(&debugger);
    
    // Start the emulation
    startEmulation(argc, argv, &emulator);
    
    return 0;
}

