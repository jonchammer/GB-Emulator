#include "Cartridge.h"

Cartridge::Cartridge() : mData(NULL), mDataSize(0), mMBC(NULL)
{
    reset();
}

Cartridge::~Cartridge()
{
    save();
    reset();
}

void Cartridge::write(const word address, const byte data)
{
    // Any writes need to be handled by the MBC. It's either a write to the ROM
    // area, in which case the MBC needs to handle banking, or a write to RAM
    // which the MBC also handles.
    mMBC->write(address, data);
}

byte Cartridge::read(const word address) const
{
    // Read from ROM Bank 0
    if (address < 0x4000)
        return mData[address];
    
    // Read from the current ROM Bank
    else if (address >= 0x4000 && address < 0x8000)
    {
        if (mMBC->getCurrentROMBank() < mInfo.numROMBanks)
            return mData[(address - 0x4000) + (mMBC->getCurrentROMBank() * 0x4000)];
        
        else 
        {
            cerr << "Illegal ROM bank read. Accessing data from ROM Bank " << mMBC->getCurrentROMBank() <<
                " when there are only " << mInfo.numROMBanks << " banks." << endl;
            return 0x00;
        }
    }
    
    // Read from the correct RAM memory bank, but only if RAM has been enabled
    else if ( (address >= 0xA000) && (address <= 0xBFFF) )
        return mMBC->read(address);
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to Cartridge." << endl;
        return 0x00;
    }
}

void Cartridge::reset()
{
    if (mData     != NULL) delete[] mData;
    if (mMBC      != NULL) delete mMBC;
    
    mData       = NULL;
    mMBC        = NULL;
    mDataSize   = 0;
    
    memset(&mInfo, 0, sizeof(CartridgeInfo));
}

bool Cartridge::load(const string& filename, const string* saveFilename)
{
    reset();

    if (loadData(filename))
    {
        cout << "Game successfully loaded: " << filename << endl;
        
        // Fill in the cartridge information
        fillInfo(filename, saveFilename);
            
        // Assign an MBC unit
        switch (mInfo.type)
        {
            case TYPE_ROM_ONLY: mMBC = new MBC0(this, mInfo.numRAMBanks); break;
            case TYPE_MBC1:     mMBC = new MBC1(this, mInfo.numRAMBanks); break;
            case TYPE_MBC2:     mMBC = new MBC2(this, mInfo.numRAMBanks); break;
            case TYPE_MBC3:     mMBC = new MBC3(this, mInfo.numRAMBanks); break;
            case TYPE_MBC5:     mMBC = new MBC5(this, mInfo.numRAMBanks); break;
        }

        // Try to load the save (but only if this cartridge actually supports saving)
        if (mInfo.hasBattery)
        {
            if (!mMBC->loadSave(mInfo.savePath))
                cerr << "Unable to load save file: " << mInfo.savePath << endl;
            else cout << "Save file loaded: " << mInfo.savePath << endl;
        }
        
        return true;
    }
    
    else return false;
}

bool Cartridge::save()
{
    return mMBC->save();
}

bool Cartridge::loadData(const string& filename)
{
    ifstream din;
    din.open(filename.c_str(), ios::binary | ios::ate);
    if (!din) return false;
    
    // Read the length of the file first
    ifstream::pos_type length = din.tellg();
    din.seekg(0, ios::beg);
    
    // Create the array that holds the contents of the ROM
    mData      = new byte[length];
    mDataSize  = length;
    
    // Read the whole block at once
    din.read( (char*) mData, length);
    din.close();
    return true;
}

void Cartridge::fillInfo(const string& filename, const string* saveFilename)
{
    mInfo.path = filename;
    
    // Save path - save file has the same name with a .sav extension.
    if (saveFilename == NULL)
    {
        int dot = filename.find_last_of('.');
        mInfo.savePath = filename.substr(0, dot) + ".sav";
    }
    else mInfo.savePath = *saveFilename;
    
    // Game name
    mInfo.name = string((char*) (mData + 0x134), 16);
    
    // Cartridge type
    switch (mData[0x147])
    {
        case 0x0:                                                         mInfo.type = TYPE_ROM_ONLY; break; 
        case 0x1:  case 0x2: case 0x3:                                    mInfo.type = TYPE_MBC1;     break;
        case 0x5:  case 0x6:                                              mInfo.type = TYPE_MBC2;     break;
        case 0x12: case 0x13: case 0xF: case 0x10: case 0x11:             mInfo.type = TYPE_MBC3;     break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: mInfo.type = TYPE_MBC5;     break;
    }
    
    // ROM Banks
    switch(mData[0x148])
    {
        case 0x00: mInfo.numROMBanks = 2;   mInfo.ROMSize = "32 KB";  break;
        case 0x01: mInfo.numROMBanks = 4;   mInfo.ROMSize = "64 KB";  break;
        case 0x02: mInfo.numROMBanks = 8;   mInfo.ROMSize = "128 KB"; break;
        case 0x03: mInfo.numROMBanks = 16;  mInfo.ROMSize = "256 KB"; break;
        case 0x04: mInfo.numROMBanks = 32;  mInfo.ROMSize = "512 KB"; break;
        case 0x05: mInfo.numROMBanks = 64;  mInfo.ROMSize = "1 MB";   break;
        case 0x06: mInfo.numROMBanks = 128; mInfo.ROMSize = "2 MB";   break;
        case 0x52: mInfo.numROMBanks = 72;  mInfo.ROMSize = "1.1 MB"; break;
        case 0x53: mInfo.numROMBanks = 80;  mInfo.ROMSize = "1.2 MB"; break;
        case 0x54: mInfo.numROMBanks = 96;  mInfo.ROMSize = "1.5 MB"; break;
    }
    
    // RAM Banks
    switch(mData[0x149])
    {
        case 0x00: mInfo.numRAMBanks = 0;  mInfo.RAMSize = "None";   break;
        case 0x01: mInfo.numRAMBanks = 1;  mInfo.RAMSize = "2 KB";   break;
        case 0x02: mInfo.numRAMBanks = 1;  mInfo.RAMSize = "8 KB";   break;
        case 0x03: mInfo.numRAMBanks = 4;  mInfo.RAMSize = "32 KB";  break;
        case 0x04: mInfo.numRAMBanks = 16; mInfo.RAMSize = "128 KB"; break;
    }
    
    // Has battery
    switch (mData[0x147])
    {
        case 0x03: case 0x06: case 0x0D: case 0x0F: case 0x10: case 0x13: case 0x1B: case 0x1E:
            mInfo.hasBattery = true; break;
        default: mInfo.hasBattery = false;
    }
    
    // GBC Support
    if ((mData[0x143] == 0x80) || (mData[0x143] == 0xC0))
        mInfo.gbc = true;
}

void Cartridge::printInfo()
{
    string typeString = "";
    switch (mInfo.type)
    {
        case TYPE_ROM_ONLY: typeString = "ROM ONLY"; break;
        case TYPE_MBC1:     typeString = "MBC1";     break;
        case TYPE_MBC2:     typeString = "MBC2";     break;
        case TYPE_MBC3:     typeString = "MBC3";     break;
        case TYPE_MBC5:     typeString = "MBC5";     break;
    }
    
    cout << "--------------------------------------------------"        << endl;
    cout << "Cartridge Name: " << mInfo.name                            << endl;
    cout << "Cartridge Type: " << typeString                            << endl;
    cout << "ROM Banks:      " << mInfo.numROMBanks                     << endl;
    cout << "ROM Size:       " << mInfo.ROMSize                         << endl;
    cout << "RAM Banks:      " << mInfo.numRAMBanks                     << endl;
    cout << "RAM Size:       " << mInfo.RAMSize                         << endl;
    cout << "Battery:        " << (mInfo.hasBattery ? "true" : "false") << endl;
    cout << "GBC Support:    " << (mInfo.gbc ? "true" : "false")        << endl;
    cout << "File Path:      " << mInfo.path                            << endl;
    cout << "Save Path:      " << mInfo.savePath                        << endl;
    cout << "--------------------------------------------------"        << endl;
}