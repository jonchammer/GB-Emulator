/* 
 * File:   RTC.h
 * Author: Jon
 *
 * Created on June 13, 2016, 6:45 PM
 */

#ifndef RTC_H
#define RTC_H

#include <ctime>
#include <iostream>
#include <fstream>
#include "Common.h"
using namespace std;

/**
 * This class represents a Real Time Clock that is used for MBC3
 * cartridges to keep time. Physical RTC units have a battery
 * supported clock that ticks, even while the game is off.
 * 
 * Read Procedure:
 * To read from the RTC, games will first latch the current time
 * into the RTC registers by writing to 0x6000-0x7FFF. Then, any
 * of the 5 registers can be safely read. (Games will first select
 * the desired register by writing 0x08-0x0C to 0x4000-0x5FFF.
 * Then they can read by reading from 0xA000-0xBFFF.)
 * 
 * Write Procedure:
 * To write to one of the RTC registers, the clock should be halted
 * first by writing X1XX XXXX when bank 0x0C is selected. Then the
 * RTC registers can safely be modified.
 */
class RTC
{
public:
    // Constructors / Destructors
    RTC();
    
    // Load / save state from a file
    bool loadState(const string& filename);
    bool saveState(const string& filename);
    
    // Interaction with cartridge
    byte readFromActiveRegister();
    void updateActiveRegister(byte data);
    void latch(byte data);
    
    // Getters / Setters
    void setActiveRegister(int reg);
    int getActiveRegister();
    
private:
    byte mRTCRegisters[5]; // The contents of the 5 RTC registers
    time_t mBaseTime;      // Offset
    time_t mHaltTime;      // When the clock was stopped
    int mRTCReg;           // The currently selected RTC register (-1 or 0x08-0x0C)
    byte mLastLatchWrite;  // Used to switch to latching mode
    
    static const int RTC_SECONDS   = 0; // Seconds [0, 59]
    static const int RTC_MINUTES   = 1; // Minutes [0, 59]
    static const int RTC_HOURS     = 2; // Hours   [0, 24]
    static const int RTC_DAYS_LOW  = 3; // Low 8 bits for Days [0, 0xFF]
    static const int RTC_DAYS_HIGH = 4; // High bit for days, carry bit, halt flag
    
    static const int FLAG_HALT     = 6; // Used for RTC_DAYS_HIGH
    static const int FLAG_OVERFLOW = 7; // Used for RTC_DAYS_HIGH
    
    static const int SECONDS_PER_DAY    = 86400;
    static const int SECONDS_PER_HOUR   = 3600;
    static const int SECONDS_PER_MINUTE = 60;
};

#endif /* RTC_H */

