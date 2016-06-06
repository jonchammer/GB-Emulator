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
#include "Debugger.h"

const word JOYPAD_STATUS_ADDRESS = 0xFF00;

class Memory;
class Debugger;

class Input : public Component
{
public:
    /**
     * Create a new Input component.
     * 
     * @param memory.   A pointer to the memory unit of the Gameboy.
     */
    Input(Memory* memory, EmulatorConfiguration* configuration);
    
    /**
     * Destructor.
     */
    virtual ~Input() {};
    
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
    
    void attachDebugger(Debugger* debugger) { mDebugger = debugger; }
    
private:
    // Allow the debugger read only access to the state of the CPU
    friend class Debugger;
    
    Memory* mMemory;      // Pointer to the main memory unit.
    Debugger* mDebugger;  // A pointer to the debugger (can be NULL)
    EmulatorConfiguration* mConfig;
            
    byte mJoypadState;    // Contains the current state of each button (1 per bit). 0 is pressed.
    byte mJoypadReg;      // The register that contains the information sent to the game
};

#endif /* INPUT_H */

