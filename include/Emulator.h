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
class Timers;
class Graphics;
class Input;
class Sound;
class Debugger;
class Emulator;

/**
 * The emulator is written such that the host environment is abstracted away.
 * SDL2, for example, can be used for rendering/sound/input on desktops, but
 * platform-specific modules can be used for mobile devices. The main function
 * will use this function as an entry point for those systems. Any system-specific
 * logic will go here.
 * 
 * @param argc.     From main.
 * @param argv.     From main.
 * @param emulator. A pointer to the emulator.
 */
void startEmulation(int argc, char** argv, Emulator* emulator);

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
    
    /**
     * Destructor.
     */
    ~Emulator();
    
    /**
     * This function should be called by the host environment approximately 60x
     * per second. It executes a single update of the emulator, which allows it
     * to function properly.
     */
    void update();    

    /**
     * Reset the emulator to its default state. Any games that are currently
     * loaded will need to be loaded again, as memory is also cleared.
     * 
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    void reset(bool skipBIOS = false);
    
    /**
     * Attach the given debugger to the emulator so the user can monitor code
     * execution. If the pointer is NULL, the debugger is removed.
     */
    void attachDebugger(Debugger* debugger);
    
    // Adjust 'paused' state
    void togglePaused()         { mPaused = !mPaused; }
    void setPaused(bool paused) { mPaused = paused;   }
    bool isPaused()             { return mPaused;     }
    
    // Getters
    CPU* getCPU()           { return mCPU;      }
    Memory* getMemory()     { return mMemory;   }
    Timers* getTimers()     { return mTimers;   }
    Graphics* getGraphics() { return mGraphics; }
    Input* getInput()       { return mInput;    }
    Sound* getSound()       { return mSound;    }
    
private:
    CPU* mCPU;           // The simulated CPU
    Memory* mMemory;     // The simulated memory unit
    Timers* mTimers;     // Timer registers
    Graphics* mGraphics; // Handles the scanline rendering
    Input* mInput;       // Handles user input
    Sound* mSound;       // Handles sound processing
    bool mPaused;        // True if emulation should be frozen
    
    // Keeps track of how many cycles have been processed this second
    int mCycleCount;
    
    // This function is called by the CPU for each part of an instruction.
    // We forward the information to each of the other components.
    friend class CPU;
    void sync(int cycles);
};

#endif /* EMULATOR_H */

