/* 
 * File:   MBC.h
 * Author: Jon C. Hammer
 *
 * Created on May 14, 2016, 10:12 PM
 */

#ifndef MBC_H
#define MBC_H

#include "Common.h"
#include "Cartridge.h"

class Cartridge;

// Common base class for all MBC chips. Each subclass is free to implement
// banking however is desired, but the interface is the same.
class MBC
{
public:
    // Constructors / Destructors
    MBC(Cartridge* owner, int numRAMBanks);
    virtual ~MBC();
    
    // Read / Write access
    virtual byte read(word address);
    virtual void write(word address, byte data) = 0;
    
    virtual word getCurrentROMBank() { return mCurrentROMBank; }
    virtual word getCurrentRAMBank() { return mCurrentRAMBank; }
    virtual bool isRAMEnabled()      { return mEnableRAM;      }
    
    // Save and load the contents of the RAM
    virtual bool save();
    virtual bool loadSave(const string& filename);
    
protected:
    Cartridge* mOwner;
    byte* mRAMBanks;      // The contents of the RAM banks
    int mNumRAMBanks;     // The number of RAM banks this MBC supports
    word mCurrentROMBank; // The currently selected ROM Bank
    word mCurrentRAMBank; // The currently selected RAM Bank
    bool mEnableRAM;      // True when writing to RAM has been enabled
    bool mUpdateSave;     // True when the save file can be updated.
    
    // Most MBCs use the same procedure when writing to the RAM inside
    // the cartridge
    void defaultRAMWrite(word address, byte data);
};

// "Dummy" MBC. Used for games like Tetris that don't actually use banking at all
class MBC0 : public MBC
{
public:
    MBC0(Cartridge* owner, int numRAMBanks) : MBC(owner, numRAMBanks) {}
    void write(word, byte) {}
};

// MBC 1 - 16Mbit ROM/8KB RAM or 4Mbit ROM/32KB RAM
class MBC1 : public MBC
{
public:
    MBC1(Cartridge* owner, int numRAMBanks) : MBC(owner, numRAMBanks), mROMBanking(true) {}
    void write(word address, byte data);
    
    word getCurrentROMBank()
    {
        if (mROMBanking) return mCurrentROMBank;
        else             return mCurrentROMBank & 0x1F;
    }
    
    word getCurrentRAMBank() 
    {
        if (mROMBanking) return 0;
        else             return mCurrentRAMBank;
    }
    
private:
    bool mROMBanking;
};

// MBC 2 - 2Mbit ROM. No RAM switching, but 4x512 bit RAM is contained on the
// cartridge itself.
class MBC2 : public MBC
{
public:
    MBC2(Cartridge* owner, int numRAMBanks) : MBC(owner, numRAMBanks) {}
    void write(word address, byte data);
};

// MBC 3 - Like MBC 1, but allows 16Mbit ROM. Includes Real-time clock (RTC)
class MBC3 : public MBC
{
public:
    MBC3(Cartridge* owner, int numRAMBanks) : MBC(owner, numRAMBanks), mRTCReg(-1), mLastLatchWrite(0xFF), mBaseTime(0), mHaltTime(0) {}
    
    byte read(word address);
    void write(word address, byte data);
    
    // Save and load the contents of the RAM
    bool save();
    bool loadSave(const string& filename);
    
private:
    byte mRTCRegisters[5];
    int mRTCReg;
    byte mLastLatchWrite;
    std::time_t mBaseTime;
    std::time_t mHaltTime;
    
    void latchRTCData();
    void RAMRTCWrite(word address, byte data);
};

// MBC 5 - Like MBC 3, but no RTC. Can access up to 64Mbit ROM/1 Mbit RAM
class MBC5 : public MBC
{
public:
    MBC5(Cartridge* owner, int numRAMBanks) : MBC(owner, numRAMBanks) {}
    void write(word address, byte data);
};

#endif /* MBC_H */

