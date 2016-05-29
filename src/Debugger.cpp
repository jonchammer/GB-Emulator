#include "Debugger.h"

Debugger::Debugger() : mEnabled(false), mPaused(false), mState(DEFAULT)
{
    
}

void showMenu()
{
    cout << "Commands:"                  << endl
         << "  I - Step into"            << endl
         << "  O - Step over"            << endl
         << "  P - Step out"             << endl
         << "  C - Continue"             << endl
         << "  H - Show this menu again" << endl
         << "  S - Show stack trace"     << endl
         << "  Q - Quit debugging"       << endl << endl;
}

void Debugger::executeCommand()
{
    std::string command;

    do
    {
        cout << ">>> ";
        getline(cin, command);
        
        if (command.length() > 1)
        {
            cout << "Command not recognized." << endl;
            showMenu();
            continue;
        }
        
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        switch (command[0])
        {
            case 'i': mState = DEFAULT;   return;
            case 'o': mState = STEP_OVER; return;
            case 'p': mState = STEP_OUT;  return;
            
            case 'c': 
            {
                mPaused = false;
                cout << "Continuing emulation." << endl;
                return;
            }
            case 'h':
            {
                showMenu();
                break;
            }
            case 's':
            {
                // Print the stack trace
                cout << "Stack: [ ";
                for (size_t i = 0; i < mStackTrace.size(); ++i)
                {
                    printHex(cout, mStackTrace[i]); 
                    cout << " ";
                }
                cout << "]" << endl;
                break;
            }
            case 'q':
            {
                mEnabled = false;
                cout << "Exiting debug mode." << endl;
                return;
            }
            
            default: cout << "Command not recognized." << endl; 
        }
    } while (true);
}

void Debugger::CPUUpdate()
{
    if (!mEnabled) return;
    
    // Check to see have hit one of the user-defined breakpoints
    if (!mPaused && mBreakpoints.find(mCPU->mProgramCounter) != mBreakpoints.end())
    {
        mPaused = true;
        cout << "Breakpoint hit at address: "; printHex(cout, mCPU->mProgramCounter); cout << endl;
        showMenu();
    }
    
    static byte lastOpcode = 0x00;
    static size_t targetStackSize;
    
    if (mPaused)
    {
        printState();
        
        // Manage the stack trace
        if (mStackTrace.empty())
            mStackTrace.push_back(mCPU->mProgramCounter);
        
        // Calls
        else if (lastOpcode == 0xCD)
            mStackTrace.push_back(mCPU->mProgramCounter);
        else if (lastOpcode == 0xC4 || lastOpcode == 0xCC || lastOpcode == 0xD4 || lastOpcode == 0xDC)
        {
            // Figure out where the call on the last instruction was supposed to go
            word callAddress = mMemory->read(mCPU->mProgramCounter - 1) << 8;
            callAddress     |= mMemory->read(mCPU->mProgramCounter - 2);
            
            // If our current program counter matches the call address, we know the call was
            // actually made. (Otherwise, the conditional test failed, and the call was skipped)
            if (callAddress == mCPU->mProgramCounter)
                mStackTrace.push_back(mCPU->mProgramCounter);
        }
 
        // Returns
        else if (lastOpcode == 0xC9 || lastOpcode == 0xD9)
            mStackTrace.pop_back();
        else if (lastOpcode == 0xC0 || lastOpcode == 0xC8 || lastOpcode == 0xD0 || lastOpcode == 0xD8)
        {
            // When a conditional return fails, the current program counter will be one higher than
            // the previous one (which is at the top of the stack trace). So to test if a return
            // succeeded, we need to make sure those two counters have different values.
            if (mStackTrace.back() != mCPU->mProgramCounter - 1)
                mStackTrace.pop_back();
        }
        
        mStackTrace.back() = mCPU->mProgramCounter;
        
        // Handle state transitions
        if (mState == DEFAULT)  
        {
            executeCommand();
            
            if (mState == STEP_OVER)
                targetStackSize = mStackTrace.size();
            else if (mState == STEP_OUT)
                targetStackSize = mStackTrace.size() - 1;
        }
        
        // We skip over instructions until the stack trace has the proper number
        // of elements in it. For step over, we want the number of elements to
        // be the same (+1 -> -1 -> 0). When we step out, we want to proceed
        // until we return, which reduces the stack size by 1.
        else if (mState == STEP_OVER || mState == STEP_OUT)
        {
            if (mStackTrace.size() == targetStackSize)
                executeCommand();
        }
        
        lastOpcode = mCPU->mCurrentOpcode;
    }
}

void Debugger::joypadUpdate()
{
    if (!mEnabled || !mPaused) return;
}

void Debugger::memoryRead(const word address, const byte data)
{
    if (!mEnabled || !mPaused) return;
}

void Debugger::memoryWrite(const word address, const byte data)
{
    if (!mEnabled || !mPaused) return;
}

void Debugger::setBreakpoint(const word pc)
{
    mBreakpoints.insert(pc);
}

void Debugger::printState()
{
    printf("0x%04X - [0x%04X] %-25s - AF: 0x%04X BC: 0x%04X DE: 0x%04X HL: 0x%04X SP: 0x%04X\n",
            mCPU->mProgramCounter, mCPU->mCurrentOpcode, 
            mCPU->dissassembleInstruction(mCPU->mProgramCounter).c_str(),
            mCPU->mRegisters.af, mCPU->mRegisters.bc, 
            mCPU->mRegisters.de, mCPU->mRegisters.hl, mCPU->mStackPointer.reg);
}