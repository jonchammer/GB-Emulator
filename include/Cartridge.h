/* 
 * File:   Cartridge.h
 * Author: Jon C. Hammer
 *
 * Created on May 21, 2016, 7:40 PM
 */

#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "Common.h"
#include "MBC.h"
#include "FileIO.h"
#include <fstream>
#include <cstring>
using namespace std;

// Forward declarations
class MBC;

enum CartridgeType {TYPE_ROM_ONLY, TYPE_MBC1, TYPE_MBC2, TYPE_MBC3, TYPE_MBC5};

// Represents information about a given cartridge.
struct CartridgeInfo
{
    string name;          // The name of the game
    string path;          // The path at which this cartridge was found.
    string savePath;      // The path to the save file (if applicable).
    CartridgeType type;   // The cartridge type - determines # of ROM/RAM banks.
    int numROMBanks;      // The number of ROM Banks within the cartridge.
    int numRAMBanks;      // The number of RAM Banks within the cartridge (can be 0).
    string ROMSize;       // The size of the ROM with units.
    string RAMSize;       // The size of the RAM with units.
    bool hasBattery;      // True if this cartridge has a battery (allows games to be saved)
    bool gbc;             // True if this cartridge has GBC support.
};

class Cartridge : public Component
{
public:
    
    // Constructors
    Cartridge();
    ~Cartridge();
    
    // State
    bool load(const string& filename, const string* saveFilename = NULL);
    bool save();
    void reset();
    
    // Memory access
    void write(const word address, const byte data);
    byte read(const word address) const;
    
    // Getters / Setters / Misc
    CartridgeInfo& getCartridgeInfo() { return mInfo; }
    MBC* getMBC()                     { return mMBC;  }
    void printInfo();
    
private:

    CartridgeInfo mInfo;  // Information about this ROM.
    byte* mData;          // The contents of the ROM
    int mDataSize;        // The number of elements in the 'mData' array
    MBC* mMBC;            // The MBC that handles banking.
    
    // Helpers
    bool loadData(const string& filename);
    void fillInfo(const string& filename, const string* saveFilename);
};

#endif /* CARTRIDGE_H */

