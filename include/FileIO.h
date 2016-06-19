/* 
 * File:   FileIO.h
 * Author: Jon
 *
 * Created on June 19, 2016, 10:13 AM
 */

#ifndef FILEIO_H
#define FILEIO_H

#include <fstream>
#include <iostream>
#include "Common.h"
using namespace std;

/**
 * Load a given binary file into an array of bytes. If the operation succeeds,
 * true will be returned. If it fails, false will be returned instead.
 * 
 * @param filename. The name of the file to be read.
 * @param data.     Where the data should be put (should be NULL initially, as
 *                  this function will allocate space on the heap for the data).
 * @param dataSize. The size of the data array (and how many items are in it).
 * 
 * @return true upon success, or false otherwise.
 */
bool loadBinaryFile(const string& filename, byte*& data, int& dataSize);

/**
 * Save a given array of bytes to a binary file. If the operation succeeds,
 * true will be returned. If it fails, false will be returned instead.
 * 
 * @param filename. The name of the file to be written to.
 * @param data.     The source data.
 * @param dataSize. The size of the data array (and how many items are in it).
 * 
 * @return true upon success, or false otherwise.
 */
bool saveBinaryFile(const string& filename, const byte* data, const int dataSize);

/**
 * This function pretty-prints a given array of bytes in order see the raw values.
 * 
 * @param data.      The data to be printed.
 * @param dataSize.  The size of the array (in bytes).
 * @param lineStart. Where the line counter starts.
 * @param numCols.   How many columns are displayed per row.
 */
void printHexDump(const byte* data, const int dataSize, const int lineStart = 0x0, const int numCols = 16);

#endif /* FILEIO_H */

