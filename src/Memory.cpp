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
    // The main memory is 64 KB
    mMainMemory = new byte[0x10000];
    
    // The Real Time Clock register is only used for MBC3. It takes the
    // place of the RAM banks when used.
    mRTCRegister = new byte[0x5];
    
    reset(skipBIOS);
}

Memory::~Memory()
{
    delete[] mMainMemory;
    delete[] mRTCRegister;
}

void Memory::reset(bool skipBIOS)
{
    // Initialize the main memory
    for (int i = 0; i < MAIN_MEMORY_INIT_SIZE; ++i)
        mMainMemory[MAIN_MEMORY_INIT_INDICES[i]] = MAIN_MEMORY_INIT_VALUES[i];
        
    // Start out in the BIOS
    mInBIOS = !skipBIOS;
}

byte Memory::read(word address) const
{
    // Read from BIOS memory
    if (mInBIOS && address < 0x100)
        return BIOS[address];

    // Read from the correct ROM memory bank
    else if ( address < 0x8000 )
        return mLoadedCartridge->read(address);
    
    // Read from the correct RAM memory bank
    else if ( (address >= 0xA000) && (address <= 0xBFFF) )
        return mLoadedCartridge->read(address);
        
    // Graphics access
    else if (((address >= Graphics::VRAM_START) && (address <= Graphics::VRAM_END)) ||
             ((address >= Graphics::OAM_START)  && (address <= Graphics::OAM_END))  ||
             ((address >= Graphics::LCDC)       && (address <= Graphics::WX))       ||
             ((address == Graphics::VBK))                                           ||
             ((address >= Graphics::HDMA1)      && (address <= Graphics::HDMA5))    ||
             ((address >= Graphics::BGPI)       && (address <= Graphics::OBPD)))       
        return mEmulator->getGraphics()->read(address);
        
    // Joypad I/O
    else if (address == JOYPAD_STATUS_ADDRESS)
    {
        return mEmulator->getInput()->read(address);
    }
    
    // Sound access
    else if (address >= 0xFF10 && address < 0xFF40)
    {
        return mEmulator->getSound()->read(address);
    }
    
    // Timers
    else if (address == DIVIDER_REGISTER || address == TIMA || address == TMA || address == TAC)
    {
        return mEmulator->getTimers()->read(address);
    }
    
    else if (address >= 0xFEA0 && address <= 0xFEFF)
    {
        cout << "RESTRICTED READ: "; printHex(cout, address); cout << endl;
        //return mMainMemory[address];
        return 0x00;
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
    // a bank issue. Let the cartridge deal with it.
    if (address < 0x8000) mLoadedCartridge->write(address, data);

    // Graphics access
    else if (((address >= Graphics::VRAM_START) && (address <= Graphics::VRAM_END)) ||
             ((address >= Graphics::OAM_START)  && (address <= Graphics::OAM_END))  ||
             ((address >= Graphics::LCDC)       && (address <= Graphics::WX))       ||
             ((address == Graphics::VBK))                                           ||
             ((address >= Graphics::HDMA1)      && (address <= Graphics::HDMA5))    ||
             ((address >= Graphics::BGPI)       && (address <= Graphics::OBPD))) 
        mEmulator->getGraphics()->write(address, data);
    
    // Write to the current RAM Bank
    else if ( (address >= 0xA000) && (address < 0xC000) )
        mLoadedCartridge->write(address, data);

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
        //cout << "RESTRICTED WRITE: "; printHex(cout, address); cout << endl;
        //mMainMemory[address] = data;
    }
    
    // Setting the timer attributes
    else if (address == DIVIDER_REGISTER || address == TIMA || address == TMA || address == TAC)
    {
        mEmulator->getTimers()->write(address, data);
    }

    // Joypad I/O
    else if (address == JOYPAD_STATUS_ADDRESS)
    {
        mEmulator->getInput()->write(address, data);
    }
    
    // Sound access
    else if (address >= 0xFF10 && address < 0xFF40)
        mEmulator->getSound()->write(address, data);
    
    // Unsupervised area
    else
    {
        mMainMemory[address] = data;
    }
}

void Memory::requestInterrupt(int id)
{
    byte req = read(INTERRUPT_REQUEST_REGISTER);
    req      = setBit(req, id);
    write(INTERRUPT_REQUEST_REGISTER, req);
}