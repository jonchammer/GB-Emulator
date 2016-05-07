/* 
 * File:   Emulator.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 9:49 AM
 */

#ifndef EMULATOR_H
#define EMULATOR_H

#include "Common.h"

// Forward declaration of necessary classes
class CPU;
class Memory;
class Graphics;
class Input;
class Sound;

class Emulator 
{
public:
    
    /**
     * Create a new Emulator.
     * @param skipBIOS. When true, the BIOS will be skipped. This will be
     *                  necessary for some games (if the Nintendo logo is
     *                  invalid, for example).
     */
    
    Emulator(bool skipBIOS = false);
    ~Emulator();
    
    void update();    
    void requestInterrupt(int id);

    /**
     * Reset the emulator to its default state. Any games that are currently
     * loaded will need to be loaded again, as memory is also cleared.
     * 
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    void reset(bool skipBIOS = false);
    
    // Getters
    CPU* getCPU()           { return mCPU;      }
    Memory* getMemory()     { return mMemory;   }
    Graphics* getGraphics() { return mGraphics; }
    Input* getInput()       { return mInput;    }
    Sound* getSound()       { return mSound;    }
    
private:
    CPU* mCPU;           // The simulated CPU
    Memory* mMemory;     // The simulated memory unit
    Graphics* mGraphics; // Handles the scanline rendering
    Input* mInput;       // Handles user input
    Sound* mSound;       // Handles sound processing
};

#endif /* EMULATOR_H */

