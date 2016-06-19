#include "FileIO.h"

bool loadBinaryFile(const string& filename, byte*& data, int& dataSize)
{
    ifstream din;
    din.open(filename.c_str(), ios::binary | ios::ate);
    if (!din) return false;
    
    // Read the length of the file first
    ifstream::pos_type length = din.tellg();
    din.seekg(0, ios::beg);
    
    // Create the array that holds the contents of the file
    data     = new byte[length];
    dataSize = length;
    
    // Read the whole block at once
    din.read( (char*) data, length);
    din.close();
    return true;
}

bool saveBinaryFile(const string& filename, const byte* data, const int dataSize)
{
    ofstream dout;
    dout.open(filename.c_str(), ios::out | ios::binary);
    if (!dout) return false;

    // Write the whole block at once
    dout.write((char*) data, dataSize);
    dout.close();
    return true;
}

void printHexDump(const byte* data, const int dataSize, const int lineStart, const int numCols)
{
    word line = lineStart;
   
    int i = 0;
    while (i < dataSize)
    {
        printf("%04x\t", line);
        for (int j = 0; j < numCols; ++j, ++i)
        {
            // Make sure we don't run off the end of the array in the middle
            // of a block
            if (i >= dataSize) break;
            
            printf("%02x ", data[i]);
        }
        
        printf("\n");
        line += numCols;
    }
}