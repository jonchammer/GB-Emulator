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
    //string game = "../roms/tests/OAMBug/oam_bug.gb";
    //string game = "../roms/Kirby.gb";
    string game = "../roms/Super_Mario_Land.gb";
    
    // Create the game cartridge
    Cartridge cartridge;
    if (!cartridge.load(game))
    {
        cerr << "Unable to load game: " << game << endl;
        return 1;
    }
    
    // Create the emulator and load the ROM
    Emulator emulator(true);
    emulator.getMemory()->loadCartridge(&cartridge);
    cartridge.printInfo();
  
    // Create and configure debugger
    Debugger debugger;
    debugger.setEnabled(true);
    debugger.setNumLastInstructions(200);
    //debugger.setPaused(true);
    //debugger.setJoypadBreakpoint(BUTTON_START);
    //debugger.setBreakpoint(0x67AA);
    //debugger.setBreakpoint(0x28AA);
    emulator.attachDebugger(&debugger);
    
    // Start the emulation
    startEmulation(argc, argv, &emulator);
    
    return 0;
}

