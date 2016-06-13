#include "MBC.h"
#include <ctime>

MBC::MBC(Cartridge* owner, int numRAMBanks) : 
    mOwner(owner), mNumRAMBanks(numRAMBanks), mCurrentROMBank(1), mCurrentRAMBank(0), mEnableRAM(false) 
{
    // Create the RAM Banks. Use value initialization to make sure all cells
    // are 0 at the beginning.
    mRAMBanks = new byte[numRAMBanks * 0x2000]();
}

MBC::~MBC() 
{
    if (mRAMBanks != NULL)
        delete[] mRAMBanks;

    mRAMBanks = NULL;
}
    
bool MBC::save()
{
    if (mUpdateSave) 
    {
        mUpdateSave = false;
        
        ofstream dout;
        dout.open(mOwner->getCartridgeInfo().savePath.c_str(), ios::out | ios::binary);
        if (!dout) return false;

        dout.write((char*) mRAMBanks, mNumRAMBanks * 0x2000);
        dout.close();
        return true;
    }
    
    // There is no save to update
    else return true;
}

bool MBC::loadSave(const string& filename)
{
    ifstream din;
    din.open(filename.c_str(), ios::binary | ios::ate);
    if (!din) return false;
    
    // Read the length of the file first
    ifstream::pos_type length = din.tellg();
    din.seekg(0, ios::beg);
    
    // Make sure the length of the file is correct.
    if (length < mNumRAMBanks * 0x2000)
        return false;
    
    // If the file is larger than we expect, read only the bytes we need
    din.read((char*) mRAMBanks, mNumRAMBanks * 0x2000);
    din.close();
    return true;
}

byte MBC::read(word address)
{
    if ( (address >= 0xA000) && (address <= 0xBFFF) )
    {
        if (mEnableRAM)
        {
            if (mCurrentRAMBank < mNumRAMBanks)
                return mRAMBanks[(address - 0xA000) + (mCurrentRAMBank * 0x2000)];
            
            else
            {
                cerr << "Illegal RAM bank read. Accessing data from RAM Bank " << mCurrentRAMBank <<
                " when there are only " << mNumRAMBanks << " banks." << endl;
                return 0x00;
            }
        }
        
        else return 0x00;
    }
    
    else return 0x00;
}

void MBC::defaultRAMWrite(word address, byte data)
{
    if ( (address >= 0xA000) && (address < 0xC000) )
    {
        if (mEnableRAM)
        {
            if (mCurrentRAMBank < mNumRAMBanks)
            {
                word newAddress = (address - 0xA000) + (mCurrentRAMBank * 0x2000);
                mRAMBanks[newAddress] = data;
            }
            
            else 
            {
                cerr << "Illegal RAM bank write. Accessing data from RAM Bank " << mCurrentRAMBank <<
                " when there are only " << mNumRAMBanks << " banks." << endl;
            }
        }
        
        // Writing here is a signal the game should be saved, assuming the game can
        // be saved.
        if (mOwner->getCartridgeInfo().hasBattery)
            mUpdateSave = true;
    }
}

void MBC1::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        mEnableRAM = ((data & 0x0F) == 0x0A);
    }
    
    // Do ROM Bank change
    else if (address >= 0x2000 && address < 0x4000)
    {        
        // Put the lower 5 bits into the current ROM Bank.
        // The next 2 bits (5,6) can be set in changeHiROMBank
        byte lower5 = data & 0x1F; // Isolate the lower 5 bits
        mCurrentROMBank &= 0x60;   // Disable the lower 5 bits, leaving bits 5 and 6
        mCurrentROMBank |= lower5;

        // Any access to these banks is invalid. The next bank is the one that should
        // be used
        if (mCurrentROMBank == 0x0 || mCurrentROMBank == 0x20 || mCurrentROMBank == 0x40 || mCurrentROMBank == 0x60) 
            mCurrentROMBank++;
    }
    
    // Do ROM or RAM bank change
    else if (address >= 0x4000 && address < 0x6000)
    {
        // ROM Bank change
        if (mROMBanking)
        {
            // The upper bits can only be set if there are enough ROM banks to handle it
            if (mOwner->getCartridgeInfo().numROMBanks > 32)
            {
                // Disable the upper 3 bits of the current ROM bank
                mCurrentROMBank &= 0x1F;

                // The lower 2 bits represent bits 5 and 6 of the ROM bank
                mCurrentROMBank |= ((data & 0x03) << 5);

                // Any access to these banks is invalid. The next bank is the one that should
                // be used
                if (mCurrentROMBank == 0x0 || mCurrentROMBank == 0x20 || mCurrentROMBank == 0x40 || mCurrentROMBank == 0x60) 
                    mCurrentROMBank++;
            }
        }

        // RAM Bank change
        else 
        {
            // Isolate the lower 2 bits (there are only 4 RAM banks total)
            mCurrentRAMBank = data & 0x03;  
        }
    }
    
    // This will change whether we are doing ROM banking or RAM banking
    else if (address >= 0x6000 && address < 0x8000)
    { 
        // The last bit determines if we enter ROM mode or RAM mode.
        // 0 == ROM mode, 1 == RAM mode
        mROMBanking  = ((data & 0x1) == 0);
    }
    
    // Write to the current RAM Bank, but only if RAM banking has been enabled.
    else if ((address >= 0xA000) && (address < 0xC000))
        defaultRAMWrite(address, data);
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to MBC." << endl;
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
        mEnableRAM = ((data & 0x0F) == 0x0A);
    }
    
    // Do ROM Bank change
    else if (address >= 0x2000 && address < 0x4000)
    {
        // The lower 4 bits are the desired ROM Bank (only 16 ROM banks can be used)
        mCurrentROMBank = data & 0xF;
        if (mCurrentROMBank == 0) mCurrentROMBank++;
    }
    
    else if (address < 0x8000)
    {
        // Do nothing
    }
    
    // Write to the current RAM Bank, but only if RAM banking has been enabled.
    else if ((address >= 0xA000) && (address < 0xC000))
        defaultRAMWrite(address, data);
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to MBC." << endl;
    }
}

byte MBC3::read(word address)
{
    if ( (address >= 0xA000) && (address <= 0xBFFF) && mEnableRAM)
    {
        switch (mRTCReg)
        {
            // Read from the current RAM Bank, but only if RAM banking has been enabled.
            case -1: return MBC::read(address);

            // Read from one of the RTC registers
            case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C:
                return mRTCRegisters[mRTCReg - 0x08];
        }
        
        return 0x00;
    }
    
    else return 0x00;
}

void MBC3::latchRTCData()
{
    time_t passedTime = (testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL)) - mBaseTime;

    if (passedTime > 0x1FF * 86400)
    {
        do
        {
            passedTime -= 0x1FF * 86400;
            mBaseTime  += 0x1FF * 86400;
        }
        while (passedTime > 0x1FF * 86400);

        //Day counter overflow
        mRTCRegisters[4] = setBit(mRTCRegisters[4], 7); 
    }

    mRTCRegisters[3] = (passedTime / 86400) & 0xFF;
    mRTCRegisters[4] &= 0xFE;
    mRTCRegisters[4] |= ((passedTime / 86400) & 0x100) >> 8;
    passedTime %= 86400;

    mRTCRegisters[2] = (passedTime / 3600) & 0xFF;
    passedTime %= 3600;

    mRTCRegisters[1] = (passedTime / 60) & 0xFF;
    passedTime %= 60;

    mRTCRegisters[0] = passedTime & 0xFF;
}

void MBC3::RAMRTCWrite(word address, byte data)
{
    if (mEnableRAM)
    {
        // Writing here is a signal the game should be saved, assuming the game can
        // be saved.
        if (mOwner->getCartridgeInfo().hasBattery)
            mUpdateSave = true;
        
        switch (mRTCReg)
        {
            // Write to the current RAM Bank, but only if RAM banking has been enabled.
            case -1: defaultRAMWrite(address, data); break;

            // Change one of the RTC registers
            case 0x08: 
            {
                time_t oldBaseTime = testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL);
                mBaseTime += (oldBaseTime - mBaseTime) % 60;
                mBaseTime -= data;
                mRTCRegisters[mRTCReg - 0x08] = data;
                break;
            }
            case 0x09: 
            {
                time_t oldBaseTime = testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL);
                time_t oldMinutes  = ((oldBaseTime - mBaseTime) / 60) % 60;
                mBaseTime += oldMinutes * 60;
                mBaseTime -= data * 60;
                mRTCRegisters[mRTCReg - 0x08] = data;
                break;
            }
            case 0x0A: 
            {
                time_t oldBaseTime = testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL);
                time_t oldHours    = ((oldBaseTime - mBaseTime) / 3600) % 24;
                mBaseTime += oldHours * 3600;
                mBaseTime -= data * 3600;
                mRTCRegisters[mRTCReg - 0x08] = data;
                break;
            }
            case 0x0B:
            {
                time_t oldBaseTime = testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL);
                time_t oldLowDays  = ((oldBaseTime - mBaseTime) / 86400) % 0xFF;
                mBaseTime += oldLowDays * 86400;
                mBaseTime -= data * 86400;
                mRTCRegisters[mRTCReg - 0x08] = data;
                break;
            }
            case 0x0C:
            {
                time_t oldBaseTime = testBit(mRTCRegisters[4], 6) ? mHaltTime : std::time(NULL);
                time_t oldHighDays = ((oldBaseTime - mBaseTime) / 86400) & 0x100;
                mBaseTime += oldHighDays * 86400;
                mBaseTime -= ((data & 0x1) << 8) * 86400;

                if (testBit(mRTCRegisters[4] ^ data, 6))
                {
                    if (testBit(data, 6)) mHaltTime  = std::time(NULL);
                    else                  mBaseTime += std::time(NULL) - mHaltTime;
                }

                mRTCRegisters[mRTCReg - 0x08] = data;
            }
        }
    }
}
void MBC3::write(word address, byte data)
{
    // Enable writing to RAM (and the RTC registers)
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        mEnableRAM = ((data & 0x0F) == 0x0A);
    }
    
    // Do ROM Bank change
    else if (address >= 0x2000 && address < 0x4000)
    {
        // Put the lower 7 bits into the current ROM Bank.
        byte lower7 = data & 0x7F; // Isolate the lower 7 bits
        mCurrentROMBank &= 0x80;   // Disable the lower 7 bits
        mCurrentROMBank |= lower7;
        
        // All banks can be accessed, except for 0, which is always present
        if (mCurrentROMBank == 0x0)
            mCurrentROMBank++;
    }
    
    // Do RAM bank change or RTC select
    else if (address >= 0x4000 && address < 0x6000)
    {
        // We have no interest in the upper nibble at all
        data &= 0x0F;
        
        // When the user writes a value between 0 and 3, they intend to
        // switch RAM banks
        if (data <= 0x3)
        {
            // Isolate the lower 2 bits (there are only 4 RAM banks total)
            mCurrentRAMBank = data & 0x3;
            
            // Do not use the RTC registers anymore
            mRTCReg = -1;
        }
        
        // The user wants to load one of the RTC registers into the
        // A000 - BFFF address space
        else if (data >= 0x08 && data <= 0x0C)
            mRTCReg = data;
    }
    
    // This will change whether we are doing ROM banking or RAM banking
    else if (address >= 0x6000 && address < 0x8000)
    {
        if (mRTCReg != -1)
        {
            // Latching is enabled by writing a 0, and then writing a 1
            // to this address space
            if (mLastLatchWrite == 0x00 && data == 0x01)
                latchRTCData();
            
            mLastLatchWrite = data;
        }
    }
    
    else if ((address >= 0xA000) && (address < 0xC000))
        RAMRTCWrite(address, data);
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to MBC." << endl;
    }
}

bool MBC3::save()
{
    if (mUpdateSave) 
    {
        mUpdateSave = false;
        
        // Write the normal save file
        ofstream dout;
        dout.open(mOwner->getCartridgeInfo().savePath.c_str(), ios::out | ios::binary);
        if (!dout) return false;

        dout.write((char*) mRAMBanks, mNumRAMBanks * 0x2000);
        dout.close();
        
        // Write the RTC file to store the clock state
        string rtcFilename = mOwner->getCartridgeInfo().savePath;
        int dot = rtcFilename.find_last_of('.');
        rtcFilename = rtcFilename.substr(0, dot) + ".rtc";
        
        dout.open(rtcFilename.c_str(), ios::out | ios::binary);
        if (!dout) return false;
        
        dout.write((char*) &mBaseTime, sizeof(mBaseTime));
        dout.write((char*) &mHaltTime, sizeof(mHaltTime));
        dout.write((char*) mRTCRegisters, 5);
        dout.close();
        
        return true;
    }
    
    // There is no save to update
    else return true;
}

bool MBC3::loadSave(const string& filename)
{
    // Load the save first.
    ifstream din;
    din.open(filename.c_str(), ios::binary | ios::ate);
    if (!din) return false;
    
    // Read the length of the file first
    ifstream::pos_type length = din.tellg();
    din.seekg(0, ios::beg);
    
    // Make sure the length of the file is correct.
    if (length < mNumRAMBanks * 0x2000)
        return false;
    
    // If the file is larger than we expect, read only the bytes we need
    din.read((char*) mRAMBanks, mNumRAMBanks * 0x2000);
    din.close();
    
    // Then load the RTC file if there is one.
    string rtcFilename = mOwner->getCartridgeInfo().savePath;
    int dot = rtcFilename.find_last_of('.');
    rtcFilename = rtcFilename.substr(0, dot) + ".rtc";
    
    din.open(rtcFilename.c_str(), ios::binary);
    if (!din) return false;
    
    din.read((char*) &mBaseTime, sizeof(mBaseTime));
    din.read((char*) &mHaltTime, sizeof(mHaltTime));
    din.read((char*) mRTCRegisters, 5);
    din.close();
    
    return true;
}

void MBC5::write(word address, byte data)
{
    // Enable writing to RAM
    if (address < 0x2000)
    {
        // Isolate the low nibble. 'A' means enable RAM banking, '0' means disable it.
        mEnableRAM = ((data & 0x0F) == 0x0A);
    }
    
    // The entire contents of the data (all 8 bits) are the low 8
    // bits of the rom bank. The remaining bit is in the high rom bank.
    else if (address >= 0x2000 && address < 0x3000)
    {
        mCurrentROMBank &= 0xFF00;
        mCurrentROMBank |= data;
    }
    
    // The low bit of 'data' corresponds to the 9th bit of the current ROM bank.
    else if (address >= 0x3000 && address < 0x4000)
    {  
        mCurrentROMBank &= 0xFEFF;
        data &= 0x01;
        mCurrentROMBank |= (data << 8);
    }
    
    // RAM bank switch
    else if (address >= 0x4000 && address < 0x6000)
    {
        mCurrentRAMBank = (data & 0x0F);
    }
    
    else if (address >= 0x6000 && address < 0x8000)
    {
        // Do nothing
    }
    
    // Write to the current RAM Bank, but only if RAM banking has been enabled.
    else if ((address >= 0xA000) && (address < 0xC000))
        defaultRAMWrite(address, data);
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to MBC." << endl;
    }
}