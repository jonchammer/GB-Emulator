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
using namespace std;

int main(int argc, char** argv)
{
    //string game = "../roms/tests/OAMBug/oam_bug.gb";
    string game = "../roms/Links_Awakening.gb";
    
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
  
////    emulator.getInput()->keyPressed(BUTTON_START);
////    emulator.getCPU()->setLogging(true);
////    while (true)
////    {
////        emulator.update();
////    }
//    
    // Start the emulation
    startEmulation(argc, argv, &emulator);
    
    return 0;
}

