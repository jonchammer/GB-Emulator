#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <cstdlib>

template<typename T>
class RingBuffer
{
public:

    /**
     * Creates a new RingBuffer capable of holding 'size' elements.
     * @param size. The size of the buffer.
     */
    RingBuffer(int size) : mBuffer(NULL), mSize(0), mStart(0), mEnd(0)
    {
        resize(size);
    }

    ~RingBuffer()
    {
        delete[] mBuffer;
        mBuffer = NULL;
    }

    /**
     * Returns how many elements can be stored in this buffer.
     */
    int capacity();
    
    /**
     * Returns how many elements are currently in the buffer.
     */
    int used();
    
    /**
     * Returns how many elements can still be placed in the buffer without overflowing.
     */
    int available();
    
    /**
     * Change the size of this buffer so it can hold 'newSize' elements.
     * NOTE: This deletes anything that is already in the buffer.
     */
    void resize(int newSize);
    
    /**
     * Write the data stored in 'src' to the buffer.
     */
    void write(T* src, int count);
    
    /**
     * Read the data from the buffer into 'dst'.
     */
    void read(T* dst, int count);

protected:
    T* mBuffer;  // Holds the data
    int mSize;   // The size of the buffer (+1 for a sentinel)
    int mStart;  // Where the buffer begins
    int mEnd;    // Where the buffer ends
    int mUsed;   // How many elements are currently in the buffer
};

template<typename T>
void RingBuffer<T>::resize(int newSize)
{
    // Clean up the old buffer if there is one
    if (mBuffer != NULL)
        delete[] mBuffer;

    mSize   = newSize + 1;
    mBuffer = new T[mSize];

    mStart = 0;
    mEnd   = 0;
    mUsed  = 0;
}

template<typename T>
int RingBuffer<T>::capacity()
{
    return mSize - 1;
}

template<typename T>
int RingBuffer<T>::available()
{
    return mSize - 1 - mUsed;
}

template<typename T>
int RingBuffer<T>::used()
{
    return mUsed;
}

template<typename T>
void RingBuffer<T>::write(T* src, int count)
{
    // Prevent writing to a full buffer
    int bufferSize = available();
    if (bufferSize == 0) return;

    // We can only write as many cells as we have free
    if (count > bufferSize)
        count = bufferSize;

    // Wrap around if necessary
    if (mEnd == capacity())
        mEnd = 0;

    // Write the data
    if (mEnd + count > mSize - 1)
    {
        int num = mSize - mEnd - 1;

        memcpy(mBuffer + mEnd, src, num * sizeof (T));

        mEnd   = 0;
        count -= num;
        src   += num;
        mUsed += num;
    }

    memcpy(mBuffer + mEnd, src, count * sizeof (T));
    mEnd  += count;
    mUsed += count;
    if (mEnd == mSize)
        mEnd = 0;
}

template<typename T>
void RingBuffer<T>::read(T* dst, int count)
{
    // Prevent reading from an empty buffer
    int bufferSize = mUsed;
    if (bufferSize == 0) return;

    // We can only read as many elements as we have stored already
    if (count > bufferSize)
        count = bufferSize;

    // Handle wrap around
    if (mStart == capacity())
        mStart = 0;

    if (mStart + count > mSize - 1)
    {
        int num = mSize - mStart - 1;

        memcpy(dst, mBuffer + mStart, num * sizeof (T));

        mStart = 0;
        count -= num;
        dst   += num;
        mUsed -= num;
    }

    memcpy(dst, mBuffer + mStart, count * sizeof (T));
    mStart += count;
    mUsed  -= count;
    if (mStart == mSize)
        mStart = 0;
}

#endif