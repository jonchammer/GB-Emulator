/* 
 * File:   Input.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 10, 2016, 8:36 AM
 */

#include "Input.h"

Input::Input(Memory* memory, bool skipBIOS) : mMemory(memory), mDebugger(NULL)
{
    // Attach this component to the memory at the correct locations
    mMemory->attachComponent(this, JOYPAD_STATUS_ADDRESS, JOYPAD_STATUS_ADDRESS);
    reset(skipBIOS);
}

void Input::reset(bool /*skipBIOS*/)
{
    mJoypadState = 0xFF;
    mJoypadReg   = 0xCF;
}

void Input::keyPressed(int key)
{
    // This key is already pressed. We don't have to do anything else.
    if (getBit(mJoypadState, key) == 0) return;
    
    // Clear this bit to show that it has been pressed.
    // Remember, if a key pressed its bit is 0, not 1
    mJoypadState = clearBit(mJoypadState, key);
    
    // Is this a standard button or a directional button?
    bool direction = (key <= 3);
    byte keyReq    = mJoypadReg;
    
    // If the game has asked for direction information and we have a direction
    // button that has been pressed, issue the joypad interrupt.
    if (testBit(keyReq, 4) && direction)
        mMemory->requestInterrupt(INTERRUPT_JOYPAD);
    
    // If the game has asked for button information and we have a normal
    // button that has been pressed, also issue the joypad interrupt.
    else if (testBit(keyReq, 5) && !direction)
        mMemory->requestInterrupt(INTERRUPT_JOYPAD);
    
    if (mDebugger != NULL) mDebugger->joypadUpdate();
}

void Input::keyReleased(int key)
{
    mJoypadState = setBit(mJoypadState, key);
    
    if (mDebugger != NULL) mDebugger->joypadUpdate();
}

void Input::write(const word address, const byte data)
{
    if (address == JOYPAD_STATUS_ADDRESS)
        mJoypadReg = data;
    
    else
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to Joypad." << endl;
    }  
}

byte Input::read(const word address) const
{
    if (address == JOYPAD_STATUS_ADDRESS)
    {
        byte result = mJoypadReg;

        // Set the upper 2 bits
        result |= 0xC0;

        // Directions - These are in the lower nibble
        if (testBit(result, 4))
        {
            byte topHalf = mJoypadState >> 4;
            result |= (topHalf & 0x0F);
        }

        // Standard buttons - we have to shift them from the top nibble first
        else if (testBit(result, 5))
            result |= (mJoypadState & 0x0F);        

        return result;
    }
    
    else 
    {
        cerr << "Address "; printHex(cerr, address); 
        cerr << " does not belong to Joypad." << endl;
        return 0x00;
    }
}