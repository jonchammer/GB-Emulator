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
using namespace std;

int main(int argc, char** argv)
{
    //string game = "../roms/tests/sprite_test_01/sprite_test_01.gb";
    string game = "../roms/Links_Awakening.gb";

    // The save file has the same name with a .sav extension.
    int dot = game.find_last_of('.');
    string save = game.substr(0, dot) + ".sav";
    
    // Create the emulator and load the ROM
    Emulator emulator(true);
    
    if (!emulator.getMemory()->loadCartridge(game))
    {
        cout << "Unable to load game." << endl;
        return 1;
    }
    else cout << "Game loaded: " << emulator.getMemory()->getCartridgeName() << endl;

    // Try to load the save file too.
    emulator.getMemory()->loadSave(save);
        
//    emulator.getCPU()->setLogging(true);
//    while (true)
//    {
//        emulator.update();
//    }
    
    // Start the emulation
    startEmulation(argc, argv, &emulator);
    
    return 0;
}

