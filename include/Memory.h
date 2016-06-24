/* 
 * File:   Memory.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 9:25 AM
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <iostream>
#include <fstream>
#include <vector>
#include "Common.h"
#include "EmulatorConfiguration.h"
#include "Cartridge.h"
#include "Emulator.h"
#include "Debugger.h"

using namespace std;

// Forward declaration of necessary classes
class Emulator;
class Cartridge;
class Debugger;

class Memory 
{
public:
    
    /**
     * Creates a new Gameboy memory unit.
     * 
     * @param emulator. A pointer to the hosting emulator.
     * @param configuration. The system configuration.
     */
    Memory(Emulator* emulator, EmulatorConfiguration* configuration);
    
    /**
     * Destructor
     */
    ~Memory();
    
    /**
     * Read a byte from the given address in memory.
     */
    byte read(const word address) const;
    
    /**
     * Write the given data to the given address in memory.
     */
    void write(const word address, const byte data);
    
    /**
     * Used by other components when an interrupt is needed.
     * @param id. One of the INTERRUPT_XXX constants defined
     *            in Common.h.
     */
    void requestInterrupt(int id);

    /**
     * Resets the contents of the memory to its default values. This also
     * unloads any currently loaded ROMs, so loadCartridge() will have to
     * be called again.
     */
    void reset();
    
    // Cartridge manipulation
    void loadCartridge(Cartridge* cartridge);
    Cartridge* getLoadedCartridge() { return mLoadedCartridge; }
    
    void attachComponent(Component* component, word startAddress, word endAddress);
    void attachDebugger(Debugger* debugger) { mDebugger = debugger; }
    
private:
    // Allow the debugger read only access to the state of the memory
    friend class Debugger;
    
    Emulator* mEmulator;            // The hosting emulator
    Debugger* mDebugger;            // A pointer to the debugger (can be NULL)
    Cartridge* mLoadedCartridge;    // The current loaded cartridge
    EmulatorConfiguration* mConfig; // The current emulation configuration
    
    word mComponentAddresses[100]; // Holds start, end addresses for the components (interlaced)
    Component* mComponentMap[50];  // Holds pointers to the actual components
    int mComponentIndex;           // The index of the last element in the component map
   
    byte* mInternalRAM;   // Size is 8 KB (GB) or 32 KB (GBC), range is [0xC000, 0xDFFF] and [0xE000, 0xFDFF] (echo)
    int mCurrentRAMBank;  // Used for GBC mode to select the RAM bank for [0xD000, 0xDFFF]. Each bank is 0x1000 (4 KB)
    byte* mMiscRAM;       // Size is 256 B, range is [0xFF00, 0xFFFF]. Used for unused I/O Ports, Stack Area, and Interrupts    
    bool mInBIOS;         // True if we are in the BIOS.
    
    Component* getComponentForAddress(const word address) const;
};

#endif /* MEMORY_H */

