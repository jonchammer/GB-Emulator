/* 
 * File:   Common.h
 * Author: Jon C. Hammer
 *
 * Created on April 9, 2016, 9:25 AM
 */

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <iomanip>
using namespace std;

// Typedefs
typedef unsigned char byte;
typedef unsigned short word;
typedef signed char signedByte;
typedef signed short signedWord;

// Constants
const int SCREEN_WIDTH_PIXELS  = 160;
const int SCREEN_HEIGHT_PIXELS = 144;

// Interrupt bit definitions
const int INTERRUPT_VBLANK = 0; // Signals when the emulator reaches the Vertical Blank period
const int INTERRUPT_LCD    = 1; // Signals an LCD status change
const int INTERRUPT_TIMER  = 2; // Signals a TIMA overflow
const int INTERRUPT_SERIAL = 3; // Signals a serial transfer completion
const int INTERRUPT_JOYPAD = 4; // Signals a change in the joypad state (button pressed/released)

// Interrupt registers & service registers
const word INTERRUPT_ENABLED_REGISTER = 0xFFFF; // IE
const word INTERRUPT_REQUEST_REGISTER = 0xFF0F; // IF
const word INTERRUPT_SERVICE_VBLANK   = 0x0040;
const word INTERRUPT_SERVICE_LCD      = 0x0048;
const word INTERRUPT_SERVICE_TIMER    = 0x0050;
const word INTERRUPT_SERVICE_SERIAL   = 0x0058;
const word INTERRUPT_SERVICE_JOYPAD   = 0x0060;

// Input keys
const int BUTTON_RIGHT  = 0;
const int BUTTON_LEFT   = 1;
const int BUTTON_UP     = 2;
const int BUTTON_DOWN   = 3;
const int BUTTON_A      = 4;
const int BUTTON_B      = 5;
const int BUTTON_SELECT = 6;
const int BUTTON_START  = 7;

// Defines the colors that are shown when in Gameboy mode
enum GameboyPalette
{
    GRAYSCALE = 0,
    REAL      = 1
};

enum System {GB, GBC, AUTOMATIC};

const double DISPLAY_GAMMA_NONE = 1.0;
const double DISPLAY_GAMMA_GBC  = 1.7;
const double DISPLAY_COLOR_FULL = 1.0;
const double DISPLAY_COLOR_GBC  = 0.85;

// Bit twiddling routines
#define testBit(address, bit)   (!!((address) &  (1 << (bit))))
#define setBit(address, bit)    (   (address) |  (1 << (bit)))
#define clearBit(address, bit)  (   (address) & ~(1 << (bit)))
#define toggleBit(address, bit) (   (address) ^  (1 << (bit)))
#define getBit(address, bit)    (((address) >> (bit)) & 1)

// Misc functions
#define printHex(out, opcode) ((out) << "0x" << setw(4) << setfill('0') << hex << (int) (opcode))
// Can also use printf. E.g. printf("%#06x", opcode); # adds a 0x, 0 controls leading zeros, 
// 6 tells how many total characters (including '0x'), x means lowercase letters, and X means capital letters.
// Beware, though. Printing 0 with the # flag will ignore the leading '0x'. If that's an issue, 
// just do something like this instead: "0x%04x". This guarantees the 0x will be printed.

// Shorthand for defining when we are using GBC hardware
#define GBC(config) ((config)->system == GBC)

// Components are classes that are capable of being read from or written to.
class Component
{
public:
    virtual void write(const word address, const byte data) = 0;
    virtual byte read(const word address) const = 0;
};

#endif /* COMMON_H */

