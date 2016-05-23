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
#include <unordered_map>
#include "Common.h"
#include "Cartridge.h"

using namespace std;

// Forward declaration of necessary classes
class Emulator;
class Cartridge;

class Memory 
{
public:
    
    /**
     * Creates a new Gameboy memory unit.
     * 
     * @param emulator. A pointer to the hosting emulator.
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    Memory(Emulator* emulator, bool skipBIOS);
    
    /**
     * Destructor
     */
    ~Memory();
    
    /**
     * Read a byte from the given address in memory.
     */
    byte read(word address) const;
    
    /**
     * Read a byte from the given address in memory.
     * No questions asked. Don't use unless you're sure you
     * need it. The address must be contained within the
     * main memory.
     */
    byte readNaive(word address) const;
    
    /**
     * Write the given data to the given address in memory.
     */
    void write(word address, byte data);
    
    /**
     * Used by other components when an interrupt is needed.
     * @param id. One of the INTERRUPT_XXX constants defined
     *            in Common.h.
     */
    void requestInterrupt(int id);

    /**
     * Called by the CPU to inform us that we've reached the end of the BIOS.
     */
    void exitBIOS() {mInBIOS = false;}
    
    /**
     * Resets the contents of the memory to its default values. This also
     * unloads any currently loaded ROMs, so loadCartridge() will have to
     * be called again.
     * 
     * @param skipBIOS. When true, the BIOS will be skipped.
     */
    void reset(bool skipBIOS);
    
    // Cartridge manipulation
    void loadCartridge(Cartridge* cartridge);
    Cartridge* getLoadedCartridge() { return mLoadedCartridge; }
    
    void attachComponent(Component* component, word startAddress, word endAddress);
    
private:
    Emulator* mEmulator;
    Cartridge* mLoadedCartridge;
    unordered_map<word, Component*> mComponentMap;
    
    byte* mMainMemory;      // Size is   64 KB, range is [0, 0xFFFF]
    byte* mRTCRegister;     // Size is 5 bytes. Only used for MBC3.
    
    bool mInBIOS;           // True if we are in the BIOS.
};

#endif /* MEMORY_H */

