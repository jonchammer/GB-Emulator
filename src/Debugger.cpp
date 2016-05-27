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
        else if (lastOpcode == 0xCD)
            mStackTrace.push_back(mCPU->mProgramCounter);
        else if (lastOpcode == 0xC9)
            mStackTrace.pop_back();
        
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
    cout << "PC: ";  printHex(cout, mCPU->mProgramCounter);
    cout << " Op: "; printHex(cout, mCPU->mCurrentOpcode);
    cout << " AF: "; printHex(cout, mCPU->mRegisters.af);
    cout << " BC: "; printHex(cout, mCPU->mRegisters.bc);
    cout << " DE: "; printHex(cout, mCPU->mRegisters.de);
    cout << " HL: "; printHex(cout, mCPU->mRegisters.hl);
    cout << " SP: "; printHex(cout, mCPU->mStackPointer.reg);

    cout << " p1: "; printHex(cout, mMemory->read(mCPU->mProgramCounter + 1));
    cout << " p2: "; printHex(cout, mMemory->read(mCPU->mProgramCounter + 2));
    cout << endl;
}