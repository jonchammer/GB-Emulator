/* 
 * File:   Input.h
 * Author: Jon C. Hammer
 *
 * Created on April 10, 2016, 8:36 AM
 */

#ifndef INPUT_H
#define INPUT_H

#include "Common.h"
#include "Memory.h"

class Input 
{
public:
    /**
     * Create a new Input component.
     * 
     * @param memory.   A pointer to the memory unit of the Gameboy.
     * @param emulator. A pointer to the hosting emulator.
     */
    Input(Memory* memory, Emulator* emulator);
    
    /**
     * Destructor.
     */
    ~Input();
    
    /**
     * Indicate to the system that the given key was pressed. The key should
     * be one of the BUTTON_XXX constants defined in Common.h.
     * 
     * @param key. The button that was pressed.
     */
    void keyPressed(int key);
    
    /**
     * Indicate to the system that the given key was released. The key should
     * be one of the BUTTON_XXX constants defined in Common.h.
     * 
     * @param key. The button that was released.
     */
    void keyReleased(int key);
    
    /**
     * Returns a packed representation of the current button states. (Used by
     * the memory unit).
     */
    byte getJoypadState() const;
    
    /**
     * Resets the current state of the Input component to its original value.
     */
    void reset();
    
private:
    byte mJoypadState;
    Memory* mMemory;
    Emulator* mEmulator;
};

#endif /* INPUT_H */

