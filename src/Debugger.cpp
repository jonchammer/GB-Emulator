#include "Debugger.h"
#include "Common.h"

Debugger::Debugger() : mEnabled(false), mPaused(false), mState(DEFAULT), 
    mNextPush(true), mNumLastInstructions(1), mJoypadBreakpoints(0x0)
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
         << "  M 0xXXXX - Show memory at address" << endl
         << "  N 0xXXXX 0xXX - Set memory at address to new value" << endl
         << "  R [A|F|B|C|D|E|H|L] 0xXX - Set register to new value" << endl
         << "  Q - Quit debugging"       << endl << endl;
}

void Debugger::CPUUpdate()
{
    if (!mEnabled) return;
    word currentROMBank = mMemory->getLoadedCartridge()->getMBC()->getCurrentROMBank();
    
    // Make sure the stack trace is kept up to date
    if (mNextPush)
    {
        mStackTrace.push_back({mCPU->mProgramCounter, currentROMBank});
        mNextPush = false;
    }
    else 
    {
        mStackTrace.back().address = mCPU->mProgramCounter;
        mStackTrace.back().romBank = currentROMBank;
    }
    
    // Don't allow duplicate elements (e.g. from restarts)
    if ((mStackTrace.back().address == mStackTrace[mStackTrace.size() - 2].address) &&
        (mStackTrace.back().romBank == mStackTrace[mStackTrace.size() - 2].romBank))
        mStackTrace.pop_back();
    
    // Keep track of the last N instructions
    mLastInstructions.push_back({mCPU->mProgramCounter, currentROMBank});
    if (mLastInstructions.size() > mNumLastInstructions)
        mLastInstructions.pop_front();
    
    // Check to see have hit one of the user-defined breakpoints
    if (!mPaused && hitBreakpoint(mCPU->mProgramCounter) /*&& mCPU->mRegisters.hl == 0x9AE0*/)
    {
        mPaused = true;
        cout << "Breakpoint hit at address: "; printHex(cout, mCPU->mProgramCounter); cout << endl;
        printState();
        showMenu();
    }

    static size_t targetStackSize;
    
    if (mPaused)
    {
        printState();
        
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
    }
}

void Debugger::CPUStackPush()
{
    mNextPush = true;
}

void Debugger::CPUStackPop()
{
    if (mStackTrace.size() > 1)
        mStackTrace.pop_back();
}

void Debugger::joypadDown(const int key)
{
    if (!mEnabled || mPaused) return;

    if (testBit(mJoypadBreakpoints, key))
    {
        mPaused = true;
        cout << "Button breakpoint hit." << endl;
        showMenu();
    }
}

void Debugger::joypadUp(const int /*key*/)
{
    if (!mEnabled) return;
}

void Debugger::memoryRead(const word /*address*/, const byte /*data*/)
{
    if (!mEnabled || !mPaused) return;
}

void Debugger::memoryWrite(const word /*address*/, const byte /*data*/)
{
    if (!mEnabled) return;
}

void Debugger::setBreakpoint(const word pc)
{
    mBreakpoints.push_back({pc, 0xFF});
}

void Debugger::setJoypadBreakpoint(const int button)
{
    mJoypadBreakpoints = setBit(mJoypadBreakpoints, button);
}

void Debugger::printStackTrace()
{
    if (mEnabled)
    {
        // Print the contents of the stack trace vector
        cout << "Stack: [ ";
        for (size_t i = 0; i < mStackTrace.size(); ++i)
            printf("%d:0x%04x ", mStackTrace[i].romBank, mStackTrace[i].address);
        cout << "]" << endl;
    }
}

void Debugger::printLastInstructions()
{
    if (mEnabled)
    {
        printf("Last %zu Instructions:\n", mLastInstructions.size());
        list<Instruction>::const_iterator it;
        int i = 1;

        for (it = mLastInstructions.begin(); it != mLastInstructions.end(); ++it, ++i)
            printf("%04d - %d:0x%04x\n", i, it->romBank, it->address);
    }
}

void Debugger::executeCommand()
{
    std::string command;

    do
    {
        cout << ">>> ";
        getline(cin, command);
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
                printStackTrace();
                break;
            }
            case 'm':
            {
                // Print the value in memory of the current address.
                stringstream ss(command);
                string address;
                ss >> address >> address;
                word addr = (word) stoul(address, NULL, 16);
                printf("[0x%04X] = 0x%02X\n", addr, mMemory->read(addr));
                break;
            }
            case 'n':
            {
                // Modify the given value in memory
                stringstream ss(command);
                string address, data;
                ss >> address >> address >> data;
                word addr = (word) stoul(address, NULL, 16);
                byte dat  = (byte) stoul(data, NULL, 16);
                mMemory->write(addr, dat);
                break;
            }
            case 'r':
            {
                stringstream ss(command);
                string reg, val;
                ss >> reg >> reg >> val;

                byte data = (byte) stoul(val, NULL, 16);
                
                if      (reg == "a") mCPU->mRegisters.a = data;
                else if (reg == "f") mCPU->mRegisters.f = data;
                else if (reg == "b") mCPU->mRegisters.b = data;
                else if (reg == "c") mCPU->mRegisters.c = data;
                else if (reg == "d") mCPU->mRegisters.d = data;
                else if (reg == "e") mCPU->mRegisters.e = data;
                else if (reg == "h") mCPU->mRegisters.h = data;
                else if (reg == "l") mCPU->mRegisters.l = data;
                
                break;
            }
            case 'q':
            {
                mEnabled = false;
                cout << "Exiting debug mode." << endl;
                return;
            }
            
            default: 
            {
                cout << "Command not recognized." << endl; 
                showMenu();
            }
        }
    } while (true);
}

void Debugger::printState()
{
    for (size_t i = 1; i < mStackTrace.size(); ++i)
        printf("..");
    
    printf("0x%04X - [%d:0x%04X] %-25s - AF: 0x%04X BC: 0x%04X DE: 0x%04X HL: 0x%04X SP: 0x%04X\n",
            mCPU->mProgramCounter, mMemory->getLoadedCartridge()->getMBC()->getCurrentROMBank(), mCPU->mCurrentOpcode, 
            mCPU->dissassembleInstruction(mCPU->mProgramCounter).c_str(),
            mCPU->mRegisters.af, mCPU->mRegisters.bc, 
            mCPU->mRegisters.de, mCPU->mRegisters.hl, mCPU->mStackPointer.reg);
}

bool Debugger::hitBreakpoint(const word pc)
{
    word currentROMBank = mMemory->getLoadedCartridge()->getMBC()->getCurrentROMBank();
    
    for (size_t i = 0; i < mBreakpoints.size(); ++i)
    {
        if (mBreakpoints[i].address == pc)
        {
            if (mBreakpoints[i].romBank == 0xFF || mBreakpoints[i].romBank == currentROMBank)
                return true;
        }
    }
    
    return false;
}