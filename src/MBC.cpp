#include "MBC.h"

void MBC1::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        byte testData = data & 0xF;
        if (testData == 0xA)
            mEnableRAM = true;
        else if (testData == 0x0)
            mEnableRAM = false;
    }
    
    // Do ROM Bank change
    else if ( (address >= 0x2000) && (address < 0x4000) )
    {
        // Put the lower 5 bits into the current ROM Bank.
        // The next 2 bits (5,6) can be set in changeHiROMBank
        byte lower5 = data & 0x1F; // Isolate the lower 5 bits
        mCurrentROMBank &= 0xE0;   // Disable the lower 5 bits
        mCurrentROMBank |= lower5;
        
        // Any access to these banks is invalid. The next bank is the one that should
        // be used
        if (mCurrentROMBank == 0x0 || mCurrentROMBank == 0x20 || mCurrentROMBank == 0x40 || mCurrentROMBank == 0x60) 
            mCurrentROMBank++;
    }
    
    // Do ROM or RAM bank change
    else if ( (address >= 0x4000) && (address < 0x6000) )
    {
        // ROM Bank change
        if (mROMBanking)
        {
            // Disable the upper 3 bits of the current ROM bank
            mCurrentROMBank &= 0x1F;

            // Disable the lower 5 bits of the data (leaving the upper 3 bits))
            data &= 0xE0;
            mCurrentROMBank |= data;

            // Any access to these banks is invalid. The next bank is the one that should
            // be used
            if (mCurrentROMBank == 0x0 || mCurrentROMBank == 0x20 || mCurrentROMBank == 0x40 || mCurrentROMBank == 0x60) 
                mCurrentROMBank++;
        }

        // RAM Bank change
        else 
        {
            // Isolate the lower 2 bits (there are only 4 RAM banks total)
            mCurrentRAMBank = data & 0x3;
        }
    }
    
    // This will change whether we are doing ROM banking or RAM banking
    else if ( (address >= 0x6000) && (address < 0x8000) )
    {
        // The last bit determines if we enter ROM mode or RAM mode.
        // 0 == ROM mode, 1 == RAM mode
        byte newData = data & 0x1;
        mROMBanking  = (newData == 0);
        
        // When in ROM Mode, the gameboy can only use RAM bank 0
        if (mROMBanking)
            mCurrentRAMBank = 0;
    }
}

void MBC2::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Bit 4 of the address must be 0.
        if (testBit(address, 4) == 1) return;
        
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        byte testData = data & 0xF;
        if (testData == 0xA)
            mEnableRAM = true;
        else if (testData == 0x0)
            mEnableRAM = false;
    }
    
    // Do ROM Bank change
    else if ( (address >= 0x2000) && (address < 0x4000) )
    {
        // The lower 4 bits are the desired ROM Bank (only 16 ROM banks can be used)
        mCurrentROMBank = data & 0xF;
        if (mCurrentROMBank == 0) mCurrentROMBank++;
    }
}

void MBC3::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        byte testData = data & 0xF;
        if (testData == 0xA)
            mEnableRAM = true;
        else if (testData == 0x0)
            mEnableRAM = false;
    }
    
    // Do ROM Bank change
    else if ( (address >= 0x2000) && (address < 0x4000) )
    {
        // Put the lower 7 bits into the current ROM Bank.
        byte lower7 = data & 0x7F; // Isolate the lower 7 bits
        mCurrentROMBank &= 0x80;   // Disable the lower 7 bits
        mCurrentROMBank |= lower7;
        
        // All banks can be accessed, except for 0, which is always present
        if (mCurrentROMBank == 0x0)
            mCurrentROMBank++;
    }
    
    // Do RAM bank change
    else if ( (address >= 0x4000) && (address < 0x6000) )
    {
        // Isolate the lower 2 bits (there are only 4 RAM banks total)
        mCurrentRAMBank = data & 0x3;
    }
    
    // This will change whether we are doing ROM banking or RAM banking
    else if ( (address >= 0x6000) && (address < 0x8000) )
    {
        // TODO: RTC Stuff
    }
}

void MBC5::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        byte testData = data & 0xF;
        if (testData == 0xA)
            mEnableRAM = true;
        else if (testData == 0x0)
            mEnableRAM = false;
    }
    
    // Low 8 bits of ROM bank number
    else if (address >= 0x2000 && address < 0x3000)
    {
        // The entire contents of the data (all 8 bits) are the low 8
        // bits of the rom bank. The remaining bit is in the high rom bank.
        mCurrentROMBank &= 0xFF00;
        mCurrentROMBank |= data;
    }
    
    // Top 1 bit of ROM bank number
    else if (address >= 0x3000 && address < 0x4000)
    {
        // The low bit of 'data' corresponds to the 9th bit of the current ROM bank.
        mCurrentROMBank &= 0xFEFF;
        data &= 0x01;
        mCurrentROMBank |= (data << 8);
    }
    
    // RAM bank switch
    else if (address >= 0x4000 && address < 0x6000)
    {
        mCurrentRAMBank = data & 0x0F;
    }
}