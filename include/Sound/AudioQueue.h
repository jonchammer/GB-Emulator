#ifndef AUDIOQUEUE_H
#define AUDIOQUEUE_H

#include "SDL.h"
#include "RingBuffer.h"
#include <algorithm>
#include <iostream>
using namespace std;

class AudioQueue
{
public:

    AudioQueue(int sampleRate, int sampleBufferLength, int sampleBufferPeriods)
    {
        //Allocating space for multiple buffers to avoid hiccups. For a short time SDL can keep producing sound even if emulator can't keep up.
        mSampleBuffersQueue = new RingBuffer<short>(sampleBufferLength * sampleBufferPeriods);

        // Initialize SDL Audio information
        SDL_zero(mAudioSpec);
        mAudioSpec.freq     = sampleRate;
        mAudioSpec.format   = AUDIO_S16SYS;
        mAudioSpec.channels = 2;
        mAudioSpec.samples  = sampleBufferLength / 2;
        mAudioSpec.callback = soundSynthesizer;
        mAudioSpec.userdata = this;
        SDL_OpenAudio(&mAudioSpec, NULL);

        mMutex  = SDL_CreateMutex();
        mSignal = SDL_CreateCond();

        // Unpause
        SDL_PauseAudio(0);
    }

    ~AudioQueue()
    {
        // Pause
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        
        SDL_DestroyMutex(mMutex);
        SDL_DestroyCond(mSignal);

        delete mSampleBuffersQueue;
    }

    /**
     * Store sample buffers in a ring buffer - buffers written one after another. SDL will grab samples when it needs them.
     * 
     * @param buffer. The sample buffer to be added to the queue.
     * @param length. The length of the sample buffer.
     * @param sync. If true, this segment will wait until space becomes available. This intrinsically locks the FPS
     *              of the emulation to ~60 FPS.
     */
    void append(const short *buffer, int length, bool sync = true)
    {
        SDL_LockMutex(mMutex);
        if (mSampleBuffersQueue->available() < length && sync)
        {
            // This is used in case we reached the end of the ring buffer - emulator wait for SDL callback to grab samples and free some space. 
            // Usually in this case we should overwrite old data in ring buffer. That's how a ring buffer works.
            // Because this is sound we are talking about overwriting will corrupt sound, though.
            SDL_CondWait(mSignal, mMutex); 

            //Also as a bonus this blocking works as frame rate lock at ~60 fps. We don't need explicit delays to keep emulator working at the right speed.
        }
        mSampleBuffersQueue->write((short*) buffer, length);
        SDL_UnlockMutex(mMutex);
    }

private:

    /**
     * This is the callback that SDL uses when it needs more sound samples. It
     * reads the data from the buffer into the given stream, possibly alerting
     * 'append' that more space is now available.
     * 
     * @param udata.  The context given by SDL (in this case, a pointer to 'this')
     * @param stream. Where the samples should be written.
     * @param len.    How many samples should be written.
     */
    static void soundSynthesizer(void *udata, unsigned char *stream, int len)
    {
        AudioQueue *self = (AudioQueue*) udata;

        // Copy data from the queue into the SDL stream
        SDL_LockMutex(self->mMutex);        
            self->mSampleBuffersQueue->read((short*) stream, std::min<int>(len / sizeof (short), self->mSampleBuffersQueue->used()));
        SDL_UnlockMutex(self->mMutex);

        // Let the 'append' method know that more space is available
        SDL_CondSignal(self->mSignal);
    }

    RingBuffer<short> *mSampleBuffersQueue; // Holds the sound buffers after they've been generated, but before SDL gets them.
    SDL_AudioSpec mAudioSpec;               // Tells SDL how the sound is structured.
    SDL_mutex *mMutex;                      // Ensure we have thread-safe access to the queue.
    SDL_cond *mSignal;                      // Used by the synthesizer to tell 'append' that space is now available.
};


#endif