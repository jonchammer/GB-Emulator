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
     */
    Input(Memory* memory);
    
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
    
    // Memory Access
    void write(const word address, const byte data);
    byte read(const word address) const;
        
    /**
     * Resets the current state of the Input component to its original value.
     */
    void reset();
    
private:
    Memory* mMemory;      // Pointer to the main memory unit.
    byte mJoypadState;    // Contains the current state of each button (1 per bit). 0 is pressed.
    byte mJoypadReg;      // The register that contains the information sent to the game
};

#endif /* INPUT_H */

