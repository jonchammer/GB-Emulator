/* 
 * File:   MBC.h
 * Author: Jon C. Hammer
 *
 * Created on May 14, 2016, 10:12 PM
 */

#ifndef MBC_H
#define MBC_H

#include "Common.h"

// Common base class for all MBC chips. Each subclass is free to implement
// banking however is desired, but the interface is the same.
class MBC
{
public:
    MBC() : mCurrentROMBank(1), mCurrentRAMBank(0), mEnableRAM(false) {}
    
    virtual void write(word address, byte data) = 0;
    
    word getCurrentROMBank() { return mCurrentROMBank; }
    word getCurrentRAMBank() { return mCurrentRAMBank; }
    bool isRAMEnabled()      { return mEnableRAM;      }
protected:
    word mCurrentROMBank;
    word mCurrentRAMBank;
    bool mEnableRAM;
};

// "Dummy" MBC. Used for games like Tetris that don't actually use banking at all
class MBC0 : public MBC
{
public:
    void write(word address, byte data) {}
};

// MBC 1 - 16Mbit ROM/8KB RAM or 4Mbit ROM/32KB RAM
class MBC1 : public MBC
{
public:
    MBC1() : mROMBanking(true) {}
    void write(word address, byte data);
private:
    bool mROMBanking;
};

// MBC 2 - 2Mbit ROM. No RAM switching, but 4x512 bit RAM is contained on the
// cartridge itself.
class MBC2 : public MBC
{
public:
    void write(word address, byte data);
};

// MBC 3 - Like MBC 1, but allows 16Mbit ROM. Includes Real-time clock (RTC)
class MBC3 : public MBC
{
public:
    void write(word address, byte data);
};

// MBC 5 - Like MBC 3, but no RTC. Can access up to 64Mbit ROM/1 Mbit RAM
class MBC5 : public MBC
{
public:
    void write(word address, byte data);
};

#endif /* MBC_H */

