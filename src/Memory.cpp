/*
 * File:   Memory.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 9:25 AM
 */

#include "Memory.h"

// The size of MAIN_MEMORY_INIT_INDICES and MAIN_MEMORY_INIT_VALUES
const int MAIN_MEMORY_INIT_SIZE = 31;

// The locations in memory that need to be set
const int MAIN_MEMORY_INIT_INDICES[] = 
{
    0xFF05, 0xFF06, 0xFF07, 0xFF10, 0xFF11, 0xFF12,
    0xFF14, 0xFF16, 0xFF17, 0xFF19, 0xFF1A, 0xFF1B,
    0xFF1C, 0xFF1E, 0xFF20, 0xFF21, 0xFF22, 0xFF23,
    0xFF24, 0xFF25, 0xFF26, 0xFF40, 0xFF42, 0xFF43,
    0xFF45, 0xFF47, 0xFF48, 0xFF49, 0xFF4A, 0xFF4B,
    0xFFFF
};

// The values corresponding to the indices in MAIN_MEMORY_INIT_INDICES
const byte MAIN_MEMORY_INIT_VALUES[] = 
{
    0x00, 0x00, 0x00, 0x80, 0xBF, 0xF3,
    0xBF, 0x3F, 0x00, 0xBF, 0x7F, 0xFF,
    0x9F, 0xBF, 0xFF, 0x00, 0x00, 0xBF,
    0x77, 0xF3, 0xF1, 0x91, 0x00, 0x00,
    0x00, 0xFC, 0xFF, 0xFF, 0x00, 0x00,
    0x00
};

// The Gameboy BIOS
const byte BIOS[] = 
{
    0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
    0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
    0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
    0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
    0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
    0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
    0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
    0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
    0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
    0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3c, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x4C,
    0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
    0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};

Memory::Memory(Emulator* emulator, bool skipBIOS) : mEmulator(emulator)
{
    // Gameboy cartridges can be at most 2048 KB / 2 MB
    mCartridgeMemory = new byte[0x200000];
    
    // The main memory is 64 KB
    mMainMemory = new byte[0x10000];
    
    // There are 4 RAM banks, each of which is 8KB, for a total of 32KB.
    // Note: There are no RAM banks when MBC2 is used.
    mRAMBanks = new byte[0x8000];
    
    // The Real Time Clock register is only used for MBC3. It takes the
    // place of the RAM banks when used.
    mRTCRegister = new byte[0x5];
    
    reset(skipBIOS);
}

Memory::~Memory()
{
    updateSave();
    
    delete[] mCartridgeMemory;
    delete[] mMainMemory;
    delete[] mRAMBanks;
    delete[] mRTCRegister;
    if (mMBC != NULL) delete mMBC;
}

void Memory::reset(bool skipBIOS)
{
    // Initialize the main memory
    for (int i = 0; i < MAIN_MEMORY_INIT_SIZE; ++i)
        mMainMemory[MAIN_MEMORY_INIT_INDICES[i]] = MAIN_MEMORY_INIT_VALUES[i];
    
    // Initialize the ROM banking information
    mMBC = NULL;
    
    // Initialize timing information
    mDividerCounter  = 0;
    mTimerPeriod     = 1024;
    mTimerCounter    = 0;
    
    // Start out in the BIOS
    mInBIOS = !skipBIOS;
    
    // Save information
    mUpdateSave = false;
    mSaveName   = "";
}

byte Memory::read(word address) const
{
    // Read from BIOS memory
    if (mInBIOS && address < 0x100)
    {
        return BIOS[address];
    }
    
    // Read from the correct ROM memory bank
    else if ( (address >= 0x4000) && (address <= 0x7FFF) )
    {
        word newAddress = address - 0x4000;
        return mCartridgeMemory[newAddress + (mMBC->getCurrentROMBank() * 0x4000)];
    }
    
    // Read from the correct RAM memory bank
    else if ( (address >= 0xA000) && (address <= 0xBFFF) )
    {
        if (mMBC->isRAMEnabled())
        {
            word newAddress = address - 0xA000;
            return mRAMBanks[newAddress + (mMBC->getCurrentRAMBank() * 0x2000)];
        }
        else return 0x00;
    }
        
    // Graphics access
    else if (((address >= Graphics::VRAM_START) && (address <= Graphics::VRAM_END)) ||
             ((address >= Graphics::OAM_START)  && (address <= Graphics::OAM_END))  ||
             ((address >= Graphics::LCDC)       && (address <= Graphics::WX))       ||
             ((address == Graphics::VBK))                                           ||
             ((address >= Graphics::HDMA1)      && (address <= Graphics::HDMA5))    ||
             ((address >= Graphics::BGPI)       && (address <= Graphics::OBPD)))       
        return mEmulator->getGraphics()->read(address);
        
    // Trap to hook in to joypad
    else if (address == JOYPAD_STATUS_ADDRESS)
    {
        return mEmulator->getInput()->getJoypadState();
    }
    
    // Sound access
    else if (address >= 0xFF10 && address < 0xFF40)
    {
        return mEmulator->getSound()->read(address);
    }
    
    // Read from main memory
    else return mMainMemory[address];
}

byte Memory::readNaive(word address) const
{
    return mMainMemory[address];
}

void Memory::write(word address, byte data)
{
    // An attempt to write to ROM means that we're dealing with
    // a bank issue. We need to handle it.
    if (address < 0x8000) mMBC->write(address, data);

    // Graphics access
    else if (((address >= Graphics::VRAM_START) && (address <= Graphics::VRAM_END)) ||
             ((address >= Graphics::OAM_START)  && (address <= Graphics::OAM_END))  ||
             ((address >= Graphics::LCDC)       && (address <= Graphics::WX))       ||
             ((address == Graphics::VBK))                                           ||
             ((address >= Graphics::HDMA1)      && (address <= Graphics::HDMA5))    ||
             ((address >= Graphics::BGPI)       && (address <= Graphics::OBPD))) 
        mEmulator->getGraphics()->write(address, data);
    
    // Write to the current RAM Bank, but only if RAM banking has been enabled.
    else if ( (address >= 0xA000) && (address < 0xC000) )
    {
        if (mMBC->isRAMEnabled())
        {
            word newAddress = address - 0xA000;
            mRAMBanks[newAddress + (mMBC->getCurrentRAMBank() * 0x2000)] = data;
        }
        
        // Writing here is a signal the game should be saved.
        mUpdateSave = true;
    }

    // Writing to working RAM also writes to ECHO RAM
    else if ((address >= 0xC000) && (address < 0xE000))
    {
        mMainMemory[address] = data;
        if (address >= 0xC000 && address <= 0xDDFF)
            mMainMemory[address + 0x2000] = data;
    }

    // Writing to ECHO ram also writes to normal RAM
    else if ( (address >= 0xE000) && (address < 0xFE00) )
    {
        mMainMemory[address] = data;
        mMainMemory[address - 0x2000] = data;
    }
    
    // This area is restricted
    else if ( (address >= 0xFEA0) && (address < 0xFEFF) )
    {
        
    }
    
    // Setting the timer attributes
    else if (address == TAC)
    {
        // The frequency is specified by the lower 2 bits of TMC
        byte currentFrequency = read(TAC) & 0x3;
        mMainMemory[TAC]      = data;
        byte newFrequency     = data & 0x3;
        
        if (currentFrequency != newFrequency)
            setClockFrequency();
    }
    
    // Trap divider register
    else if (address == DIVIDER_REGISTER)
        mMainMemory[DIVIDER_REGISTER] = 0;

    // Sound access
    else if (address >= 0xFF10 && address < 0xFF40)
        mEmulator->getSound()->write(address, data);
    
    // Unsupervised area
    else
    {
        mMainMemory[address] = data;
    }
}

void Memory::writeNaive(word address, byte data)
{
    mMainMemory[address] = data;
}

void Memory::requestInterrupt(int id)
{
    byte req = read(INTERRUPT_REQUEST_REGISTER);
    req      = setBit(req, id);
    write(INTERRUPT_REQUEST_REGISTER, req);
}

void Memory::updateTimers(int cycles)
{
    handleDividerRegister(cycles);
    
    // The clock must be enabled for it to be updated
    // Bit 2 of TMC determines whether the clock is enabled or not
    if (testBit(read(TAC), 2))
    {
        mTimerCounter += cycles;
        
        // The timer should be updated
        if (mTimerCounter >= mTimerPeriod)
        {
            int passedPeriods = mTimerCounter / mTimerPeriod;
            mTimerCounter %= mTimerPeriod;
            
            // Timer is about to overflow
            word TIMAVal = read(TIMA);
            if (TIMAVal + passedPeriods >= 255)
            {
                write(TIMA, TIMAVal + passedPeriods + read(TMA));
                requestInterrupt(INTERRUPT_TIMER);
            }
            
            // Otherwise update the timer
            else write(TIMA, TIMAVal + passedPeriods);
        }
    }
}

void Memory::setClockFrequency()
{
    // The frequency is specified by the lower 2 bits of TMC
    byte freq = read(TAC) & 0x3;
    switch (freq)
    {
        case 0 : mTimerPeriod = 1024; break; // Freq = 4096
        case 1 : mTimerPeriod = 16;   break; // Freq = 262144
        case 2 : mTimerPeriod = 64;   break; // Freq = 65536
        case 3 : mTimerPeriod = 256;  break; // Freq = 16382
    }
}

void Memory::handleDividerRegister(int cycles)
{
    mDividerCounter += cycles;
    if (mDividerCounter >= 255)
    {
        mDividerCounter = 0;
        mMainMemory[DIVIDER_REGISTER]++;
    }
}

bool Memory::loadCartridge(const string& filename)
{    
    ifstream din;
    din.open(filename.c_str(), ios::binary | ios::ate);
    if (!din)
    {
        cerr << "Unable to open file: " << filename << endl;
        return false;
    }
    
    // Read the length of the file first
    ifstream::pos_type length = din.tellg();
    din.seekg(0, ios::beg);
    
    // Read the whole block at once
    din.read( (char*) mCartridgeMemory, length);
    din.close();
    
    if (mMBC != NULL) delete mMBC;
    
    // Figure out which sort of ROM banking is used (if any)
    switch (mCartridgeMemory[0x147])
    {
        //No banking is used (e.g. Tetris)
        case 0x0: 
        {
            mMBC = new MBC0(); 
            break; 
        }
        
        // MBC 1
        case 0x1: case 0x2: case 0x3:
        {
            mMBC = new MBC1(); 
            break;
        }
            
        // MBC 2
        case 0x5: case 0x6:
        {
            mMBC = new MBC2(); 
            break;
        }

        // MBC 3
        case 0x12: case 0x13: case 0xF: case 0x10: case 0x11:
        {
            mMBC = new MBC3(); 
            break;
        }
        
        // MBC 5
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
        {
            mMBC = new MBC5(); 
            break;
        }
        
        default: 
        {
            cerr << "Cartridge Type: UNKOWN";
            printHex(cerr, mCartridgeMemory[0x147]);
            cerr << endl;
        }
    }

    // Copy the first 0x8000 bytes from the cartridge into main memory
    memcpy(&mMainMemory[0x0], &mCartridgeMemory[0x0], 0x8000);
    
    return true;
}

void Memory::printCartridgeInfo()
{
    string name     = getCartridgeName();
    string type     = "UNKNOWN";
    int numROMBanks = 0;
    int numRAMBanks = 0;
    string ROMSize  = "";
    string RAMSize  = "";
    bool gbcGame    = false;
    
    // Cartridge type
    switch (mCartridgeMemory[0x147])
    {
        case 0x0:                                                         type = "ROM Only"; break; 
        case 0x1:  case 0x2: case 0x3:                                    type = "MBC1";     break;
        case 0x5:  case 0x6:                                              type = "MBC2";     break;
        case 0x12: case 0x13: case 0xF: case 0x10: case 0x11:             type = "MBC3";     break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: type = "MBC5";     break;
    }
    
    // ROM Banks
    switch(mCartridgeMemory[0x148])
    {
        case 0x00: numROMBanks = 2;   ROMSize = "32 KB";  break;
        case 0x01: numROMBanks = 4;   ROMSize = "64 KB";  break;
        case 0x02: numROMBanks = 8;   ROMSize = "128 KB"; break;
        case 0x03: numROMBanks = 16;  ROMSize = "256 KB"; break;
        case 0x04: numROMBanks = 32;  ROMSize = "512 KB"; break;
        case 0x05: numROMBanks = 64;  ROMSize = "1 MB";   break;
        case 0x06: numROMBanks = 128; ROMSize = "2 MB";   break;
        case 0x52: numROMBanks = 72;  ROMSize = "1.1 MB"; break;
        case 0x53: numROMBanks = 80;  ROMSize = "1.2 MB"; break;
        case 0x54: numROMBanks = 96;  ROMSize = "1.5 MB"; break;
    }
    
    // RAM Banks
    switch(mCartridgeMemory[0x149])
    {
        case 0x00: numRAMBanks = 0;  RAMSize = "None";   break;
        case 0x01: numRAMBanks = 1;  RAMSize = "2 KB";   break;
        case 0x02: numRAMBanks = 1;  RAMSize = "8 KB";   break;
        case 0x03: numRAMBanks = 4;  RAMSize = "32 KB";  break;
        case 0x04: numRAMBanks = 16; RAMSize = "128 KB"; break;
    }
    
    if ((mCartridgeMemory[0x143] == 0x80) || (mCartridgeMemory[0x143] == 0xC0))
        gbcGame = true;
        
    cout << "----------------------------------------" << endl;
    cout << "Cartridge Name: " << name << endl;
    cout << "Cartridge Type: " << type << endl;
    cout << "ROM Banks:      " << numROMBanks << endl;
    cout << "ROM Size:       " << ROMSize     << endl;
    cout << "RAM Banks:      " << numRAMBanks << endl;
    cout << "RAM Size:       " << RAMSize     << endl;
    cout << "GBC Support:    " << (gbcGame ? "true" : "false") << endl;
    cout << "----------------------------------------" << endl;
}

string Memory::getCartridgeName()
{
    return string((char*) (mCartridgeMemory + 0x134), 16);
}

bool Memory::loadSave(const string& filename)
{
    mSaveName = filename;
    ifstream din;
    din.open(filename.c_str(), ios::in | ios::binary);
    if (!din)
    {
        cerr << "Unable to open file: " << filename << endl;
        return false;
    }
    
    din.read((char*) mRAMBanks, 0x8000);
    din.close();
    return true;
}
    
bool Memory::saveGame(const string& filename)
{
    ofstream dout;
    dout.open(filename.c_str(), ios::out | ios::binary);
    if (!dout)
    {
        cerr << "Unable to open file: " << filename << endl;
        return false;
    }
    
    dout.write((char*) mRAMBanks, 0x8000);
    dout.close();
    return true;
}