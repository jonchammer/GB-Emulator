/* 
 * File:   Common.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 9:25 AM
 */

#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <iomanip>
using namespace std;

// Typedefs
typedef unsigned char byte;
typedef unsigned short word;
typedef char signedByte;
typedef short signedWord;

// Constants
const int SCREEN_WIDTH_PIXELS  = 160;
const int SCREEN_HEIGHT_PIXELS = 144;

// Flag definitions - used with the 'f' register
const int FLAG_ZERO  = 7; // Set when result is 0
const int FLAG_NEG   = 6; // Set when op is a subtraction
const int FLAG_HALF  = 5; // Set when overflow from lower nibble to upper nibble
const int FLAG_CARRY = 4; // Set when overflow occurs

// Interrupt bit definitions
const int INTERRUPT_VBLANK = 0; // Signals when the emulator reaches the Vertical Blank period
const int INTERRUPT_LCD    = 1; // Signals an LCD status change
const int INTERRUPT_TIMER  = 2; // Signals a TIMA overflow
const int INTERRUPT_SERIAL = 3; // Signals a serial transfer completion
const int INTERRUPT_JOYPAD = 4; // Signals a change in the joypad state (button pressed/released)

// Locations of important sections in memory
const word DIVIDER_REGISTER = 0xFF04; // Location of the divider register in memory
const word TIMA             = 0xFF05; // Location of timer in memory
const word TMA              = 0xFF06; // Location of timer reset value, which will probably be 0
const word TAC              = 0xFF07; // Location of timer frequency

const word SCANLINE_ADDRESS     = 0xFF44;
const word COINCIDENCE_ADDRESS  = 0xFF45;
const word LCD_STATUS_ADDRESS   = 0xFF41;
const word LCD_CONTROL_REGISTER = 0xFF40;

const word SCROLL_Y        = 0xFF42;  // Y position of background
const word SCROLL_X        = 0xFF43;  // x position of background
const word WINDOW_Y        = 0xFF4A;  // y position of viewing area
const word WINDOW_X        = 0xFF4B;  // x position -7 of viewing area

const word BACKGROUND_PALETTE_ADDRESS = 0xFF47;  // Location of the background color palette in memory
const word SPRITE_PALETTE_ADDRESS1    = 0xFF48;  // Location of the first sprite color palette in memory
const word SPRITE_PALETTE_ADDRESS2    = 0xFF49;  // Location of the second sprite color palette in memory

const word INTERRUPT_ENABLED_REGISTER = 0xFFFF;
const word INTERRUPT_REQUEST_REGISTER = 0xFF0F;

const word INTERRUPT_SERVICE_VBLANK = 0x40;
const word INTERRUPT_SERVICE_LCD    = 0x48;
const word INTERRUPT_SERVICE_TIMER  = 0x50;
const word INTERRUPT_SERVICE_SERIAL = 0x58;
const word INTERRUPT_SERVICE_JOYPAD = 0x60;

const word JOYPAD_STATUS_ADDRESS = 0xFF00;
const word DMA_TRANSFER_ADDRESS  = 0xFF46;

// Input keys
const int BUTTON_RIGHT  = 0;
const int BUTTON_LEFT   = 1;
const int BUTTON_UP     = 2;
const int BUTTON_DOWN   = 3;
const int BUTTON_A      = 4;
const int BUTTON_B      = 5;
const int BUTTON_SELECT = 6;
const int BUTTON_START  = 7;

// Colors (ARGB)
const int WHITE      = 0xFFFFFFFF;
const int LIGHT_GRAY = 0xFFAAAAAA;
const int DARK_GRAY  = 0xFF555555;
const int BLACK      = 0xFF000000;

// Bit twiddling routines
#define testBit(address, bit)   (!!((address) &  (1 << (bit))))
#define setBit(address, bit)    (   (address) |  (1 << (bit)))
#define clearBit(address, bit)  (   (address) & ~(1 << (bit)))
#define toggleBit(address, bit) (   (address) ^  (1 << (bit)))
#define getBit(address, bit)    (((address) >> (bit)) & 1)

// Misc functions
#define printHex(out, opcode) ((out) << "0x" << setw(4) << setfill('0') << hex << (int) (opcode))
// Can also use printf. E.g. printf("%#04x", opcode); # adds a 0x, 0 controls leading zeros, 
// 4 tells how many places, x means lowercase letters, and X means capital letters.

class Component
{
public:
    virtual void write(const word address, const byte data) = 0;
    virtual byte read(const word address) const = 0;
};

// Includes
#include "Memory.h"
#include "CPU.h"
#include "Emulator.h"
#include "Timers.h"
#include "Graphics.h"
#include "Input.h"
#include "Sound.h"

#endif /* TYPES_H */

