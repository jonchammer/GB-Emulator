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
#include "Common.h"
#include "MBC.h"

using namespace std;

// Forward declaration of necessary classes
class Emulator;
class MBC;

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
     * Write the given data to the given address in memory.
     * No questions asked. Don't use unless you're sure you
     * need it. The address must be contained within the
     * main memory.
     */
    void writeNaive(word address, byte data);
    
    /**
     * Used by other components when an interrupt is needed.
     * @param id. One of the INTERRUPT_XXX constants defined
     *            in Common.h.
     */
    void requestInterrupt(int id);
    
    /**
     * Called once per instruction. Updates the clock timer and the divider register.
     */
    void updateTimers(int cycles);
    
    /**
     * Load the given .gb file into the cartrdige memory.
     * @param filename The name of the .gb file (including extension)
     * @return         True if we were able to read the cartridge. False otherwise.
     */
    bool loadCartridge(const string& filename);
    
    /**
     * Loads the given save file so the player can pick up where he left off.
     * @param filename The name of the .sav file (usually the same as the game name)
     * @return         True if the load was successful. False otherwise.
     */
    bool loadSave(const string& filename);
    
    /**
     * Saves the current state of the RAM banks into a file (.sav). 
     * @param filename The name of the .sav file (usually the same as the game name)
     * @return         True if the save was successful. False otherwise.
     */
    bool saveGame(const string& filename);
   
    /**
     * Reads the name of the game from cartridge memory into a string.
     */
    string getCartridgeName();
    
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
    
    // TODO: this whole process is hacky. See if there's a better alternative.
    void updateSave() {if (mUpdateSave) {saveGame(mSaveName); mUpdateSave = false;}}
    
private:
    Emulator* mEmulator;
    
    byte* mCartridgeMemory; // Size is 2048 KB, range is [0, 0x1F FFFF]
    byte* mMainMemory;      // Size is   64 KB, range is [0, 0xFFFF]
    byte* mRAMBanks;        // Size is   32 KB (4 banks x 8KB)
    byte* mRTCRegister;     // Size is 5 bytes. Only used for MBC3.
    
    bool mInBIOS;           // True if we are in the BIOS.
    MBC* mMBC;              // A pointer to the current MBC unit (varies based on cartridge type)

    int mTimerPeriod;       // Keeps track of the rate at which the timer updates
    int mTimerCounter;      // Current state of the timer
    int mDividerCounter;    // Keeps track of the rate at which the divider register updates
    
    bool mUpdateSave;       // When true, the save file will be updated
    string mSaveName;       // The name of the save file.
    
    // Private functions that are used to handle bank updates
    void handleBanking(word address, byte data);
    void enableRAMWriting(word address, byte data);
    void changeLoROMBank(byte data);
    void changeHiROMBank(byte data);
    void changeRAMBank(byte data);
    void changeROMRAMMode(byte data);
    
    // Private functions to handle timing updates
    void handleDividerRegister(int cycles);
    void setClockFrequency();
    
    // Private functions to handle Direct Memory Access
    void handleDMATransfer(byte data);
};

#endif /* MEMORY_H */

