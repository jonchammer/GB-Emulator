#include "Debugger.h"

Debugger::Debugger() : mEnabled(false), mPaused(false)
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
            case 'i': return;
            case 'o': return;
            case 'p': return;
            
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
    
    if (mPaused)
    {
        printState();
        executeCommand();
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