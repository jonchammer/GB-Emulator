/* 
 * File:   Input.cpp
 * Author: Jon C. Hammer
 * 
 * Created on April 10, 2016, 8:36 AM
 */

#include "Input.h"

Input::Input(Memory* memory, Emulator* emulator) : mMemory(memory), mEmulator(emulator)
{
    reset();
}

Input::~Input()
{
    // Do nothing
}

void Input::reset()
{
    mJoypadState = 0xFF;
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
    byte keyReq    = mMemory->readNaive(JOYPAD_STATUS_ADDRESS);
    
    // If the game has asked for direction information and we have a direction
    // button that has been pressed, issue the joypad interrupt.
    if (testBit(keyReq, 4) && direction)
        mMemory->requestInterrupt(INTERRUPT_JOYPAD);
    
    // If the game has asked for button information and we have a normal
    // button that has been pressed, also issue the joypad interrupt.
    else if (testBit(keyReq, 5) && !direction)
        mMemory->requestInterrupt(INTERRUPT_JOYPAD);        
}

void Input::keyReleased(int key)
{
    mJoypadState = setBit(mJoypadState, key);
}

byte Input::getJoypadState() const
{
    byte result = mMemory->readNaive(JOYPAD_STATUS_ADDRESS);
    
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
    {
        result |= (mJoypadState & 0x0F);        
    }

    return result;
}