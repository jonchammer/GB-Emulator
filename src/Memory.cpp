/*
 * File:   Memory.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 9, 2016, 9:25 AM
 */

#include "Memory.h"

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
    0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
    0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
    0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
    0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
    0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};

Memory::Memory(Emulator* emulator, EmulatorConfiguration* configuration) : mEmulator(emulator), mDebugger(NULL), mConfig(configuration)
{
    // Internal RAM is 8 KB for GB and 32 KB for GBC.
    // We just allocate 32 KB to cover both cases
    mInternalRAM = new byte[0x8000];
    
    // 256 bytes. Used for unused I/O ports, stack space, and interrupts
    mMiscRAM = new byte[0x100];
    
    mComponentIndex = 0;
    reset();
}

Memory::~Memory()
{
    delete[] mInternalRAM;
    delete[] mMiscRAM;
}

void Memory::reset()
{
    // Start out in the BIOS
    mInBIOS = !mConfig->skipBIOS;
    
    memset(mMiscRAM, 0xFF, 0x7F);
    memset(mMiscRAM + 0x80, 0x00, 0x40);
    
    // Specific initializations
    mMiscRAM[INTERRUPT_REQUEST_REGISTER - 0xFF00] = 0xE1;
    mMiscRAM[INTERRUPT_ENABLED_REGISTER - 0xFF00] = 0x00;
    mMiscRAM[0xFF4D - 0xFF00] = 0x00;
    
    if (GBC(mConfig))
    {
        // Initialize undocumented registers
        mMiscRAM[0xFF6C - 0xFF00] = 0xFE;
        mMiscRAM[0xFF72 - 0xFF00] = 0x00;
        mMiscRAM[0xFF73 - 0xFF00] = 0x00;
        mMiscRAM[0xFF74 - 0xFF00] = 0x00;
        mMiscRAM[0xFF75 - 0xFF00] = 0x8F;
        mMiscRAM[0xFF76 - 0xFF00] = 0x00;
        mMiscRAM[0xFF77 - 0xFF00] = 0x00;
    }
    
    // The current RAM bank starts out at 1, regardless of whether
    // we are in GB or GBC mode.
    mCurrentRAMBank = 1;
        
    // TODO Reset cartridge too?
}

byte Memory::read(const word address) const
{
    Component* component = NULL;
    byte data = 0x00;
    
    // Read from BIOS memory
    if (mInBIOS && address < 0x100)
        data = BIOS[address];

    // Check the component map to route most addresses to the component
    // that can actually deal with it.
    else if ((component = getComponentForAddress(address)) != NULL)
        data = component->read(address);
    
    // Read from internal RAM - Bank 0
    else if (address >= 0xC000 && address <= 0xCFFF)
        data = mInternalRAM[address - 0xC000];
    
    // Read from internal RAM - Current RAM bank
    // Current RAM bank is always 1 for GB
    else if (address >= 0xD000 && address <= 0xDFFF)
    {
        data = mInternalRAM[(mCurrentRAMBank * 0x1000) + (address - 0xD000)];
    }
    
    // Read from echo RAM, which uses the same storage as the internal RAM
    else if (address >= 0xE000 && address <= 0xFDFF)
    {
        data = mInternalRAM[address - 0xE000];
    }
    
    // This area is restricted
    else if (address >= 0xFEA0 && address <= 0xFEFF)
    {
        cout << "RESTRICTED READ: "; printHex(cout, address); cout << endl;
        data = 0x00;
    }

    // Handle double speed
    else if (address == SPEED_SWITCH_ADDRESS)
    {
        // The first bit indicates if a speed switch has been prepared.
        // The last bit indicates whether or not we are currently in double speed mode
        byte ret = 0x7E;     
        ret |= (mMiscRAM[address - 0xFF00] & 0x1);
        ret |= ((mConfig->doubleSpeed ? 1 : 0) << 7);
        return ret;
    }
    
    // Handle RAM bank reads in GBC mode
    else if (address == 0xFF70 && GBC(mConfig))
    {
        return 0xF8 | mCurrentRAMBank;
    }
    
    // Read from misc RAM - This statement is a touch convoluted, but it's required
    // for g++ to compile without a warning about the limited range of unsigned shorts
    else if ((address >= 0xFF00 && address <= 0xFFFE) || (address == 0xFFFF))
    {
        data = mMiscRAM[address - 0xFF00];
    }
    
    // Sanity check
    else
    {
        cerr << "READ: Address not recognized: "; 
        printHex(cerr, address); 
        cerr << endl;
        data = 0x00;
    }
    
    if (mDebugger != NULL) mDebugger->memoryRead(address, data);
    return data;
}

void Memory::write(const word address, const byte data)
{
    Component* component = NULL;
    
    // The last instruction in the BIOS will write register 'a' to address
    // 0xFF50. This is a signal that we have completed the boot.
    if (address == 0xFF50 && mInBIOS)
        mInBIOS = false;
    
    // Check the component map to route most addresses to the component
    // that can actually deal with it.
    else if ((component = getComponentForAddress(address)) != NULL)
        component->write(address, data);
    
    // Write to internal RAM - Bank 0
    else if (address >= 0xC000 && address <= 0xCFFF)
        mInternalRAM[address - 0xC000] = data;

    // Write to internal RAM - Current RAM bank
    // Current RAM bank is always 1 for GB
    else if (address >= 0xD000 && address <= 0xDFFF)
        mInternalRAM[(mCurrentRAMBank * 0x1000) + (address - 0xD000)] = data;
        
    // Write to echo RAM (also used for working RAM)
    else if (address >= 0xE000 && address <= 0xFDFF)
        mInternalRAM[address - 0xE000] = data;
    
    // This area is restricted
    else if (address >= 0xFEA0 && address <= 0xFEFF)
    {
        if (data != 0x0)
            printf("Restricted Write: %#04x = %#04x\n", address, data);
        
        //mMainMemory[address] = data;
    }

    // BGB leaves the upper 3 bits alone when setting the IF register. Those
    // bits are always set.
    else if (address == 0xFF0F)
        mMiscRAM[address - 0xFF00] = (data | 0xE0);
    
    // Handle double speed
    else if (address == SPEED_SWITCH_ADDRESS)
    {
        // We only care about the last bit for GBC speed switches
        mMiscRAM[address - 0xFF00] = (data & 0x01);
    }
    
    // Handle RAM bank changes in GBC mode
    else if (address == 0xFF70 && GBC(mConfig))
    {
        // Isolate the lower 3 bits
        mCurrentRAMBank = data & 0x07;
        
        // A value of 0 is illegal. Change it to 1 instead.
        if (mCurrentRAMBank == 0)
            mCurrentRAMBank = 1;
    }
        
    // Write to misc RAM - This statement is a touch convoluted, but it's required
    // for g++ to compile without a warning about the limited range of unsigned shorts
    else if ((address >= 0xFF00 && address <= 0xFFFE) || (address == 0xFFFF))  
    {
        mMiscRAM[address - 0xFF00] = data;
    }
    
    // Sanity check
    else 
    {
        cerr << "WRITE: Address not recognized: "; 
        printHex(cerr, address); 
        cerr << endl;
    }
    
    if (mDebugger != NULL) mDebugger->memoryWrite(address, data);
}

void Memory::requestInterrupt(int id)
{
    byte req = read(INTERRUPT_REQUEST_REGISTER);
    req      = setBit(req, id);
    write(INTERRUPT_REQUEST_REGISTER, req);
}

void Memory::loadCartridge(Cartridge* cartridge) 
{ 
    mLoadedCartridge = cartridge; 
    
    // Make sure to attach the cartridge to the proper addresses so it can
    // process its reads and writes.
    attachComponent(mLoadedCartridge, 0x0000, 0x7FFF); // ROM
    attachComponent(mLoadedCartridge, 0xA000, 0xBFFF); // RAM
}

void Memory::attachComponent(Component* component, word startAddress, word endAddress)
{
//    cout << "Attaching Range: "; 
//    printHex(cout, startAddress); cout << " - "; printHex(cout, endAddress); 
//    cout << " to component: " << component << endl;
    
    mComponentAddresses[2 * mComponentIndex]     = startAddress;
    mComponentAddresses[2 * mComponentIndex + 1] = endAddress;
    mComponentMap[mComponentIndex]               = component;
    mComponentIndex++;
}

Component* Memory::getComponentForAddress(const word address) const
{     
    // Go through the component lists to find one that can handle this address
    for (int i = 0, j = 0; i < mComponentIndex; i++, j += 2)
    {
        if (address >= mComponentAddresses[j] && address <= mComponentAddresses[j + 1])
            return mComponentMap[i];
    }
    
    return NULL;
}