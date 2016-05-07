/* 
 * File:   StreamingAudioQueue.h
 * Author: Jon
 *
 * Created on May 6, 2016, 9:48 AM
 */

#ifndef STREAMINGAUDIOQUEUE_H
#define STREAMINGAUDIOQUEUE_H

#include "SDL.h"
#include <iostream>
using std::cout;
using std::endl;

const double DEFAULT_TARGET_LATENCY = 7.0;

class StreamingAudioQueue
{
public:

    StreamingAudioQueue(int sampleRate, int sampleBufferLength) : mTargetLatency(DEFAULT_TARGET_LATENCY), mDelay(1)
    {
        // Initialize SDL Audio information
        SDL_zero(mAudioSpec);
        mAudioSpec.freq     = sampleRate;
        mAudioSpec.format   = AUDIO_S16SYS;
        mAudioSpec.channels = 2;
        mAudioSpec.samples  = sampleBufferLength / 2;
        mAudioSpec.callback = NULL;
        mAudioSpec.userdata = NULL;
        
        // Open the audio device that we'll be working with
        mAudioID = SDL_OpenAudioDevice(NULL, 0, &mAudioSpec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (mAudioID == 0)
        {
            cerr << "Unable to open audio device: " << SDL_GetError() << endl;
            return;
        }
        
        // Unpause
        SDL_PauseAudioDevice(mAudioID, 0);
    }

    ~StreamingAudioQueue()
    {
        SDL_ClearQueuedAudio(mAudioID);
        SDL_PauseAudioDevice(mAudioID, 1); // Pause
        SDL_CloseAudioDevice(mAudioID);
    }

    /**
     * Pass completed buffers to SDL. It will handle any necessary buffering.
     * 
     * @param buffer. The sample buffer to be added to the queue.
     * @param length. The length of the sample buffer.
     */
    void append(const short *buffer, int length)
    {      
        if (SDL_QueueAudio(mAudioID, buffer, length * sizeof(short)) == 0)
        {
            // Ideally, we want the latency to be a low number. If it gets too low, 
            // the sound becomes choppy, though. If it gets too high, the user will notice
            // a significant amount of lag. (We'd be adding samples faster than
            // they can be processed.) We dynamically tune the FPS in order to make
            // sure the latency is minimized whilst still having enough data to
            // ensure the sound isn't choppy. When the latency grows too large,
            // we slow execution down. When it gets too small, we speed it up.
            double latency = 1000.0 * SDL_GetQueuedAudioSize(mAudioID) / (16.0 * mAudioSpec.freq * mAudioSpec.channels);   
            mDelay = (latency < mTargetLatency) ? 1 : 0;
        }
        
        else cerr << "Unable to queue audio: " << SDL_GetError() << endl;
    }

    // Setters / Getters
    void setTargetLatency(double target) { mTargetLatency = target; } 
    double getTargetLatency()            { return mTargetLatency; }
    int getDelay()                       { return mDelay;         }
    
private:
    SDL_AudioDeviceID mAudioID; // The ID of the sound device
    SDL_AudioSpec mAudioSpec;   // Tells SDL how the sound is structured.
    
    double mTargetLatency;      // The FPS of the system will be adjusted such that 
                                // the actual latency matches this as close as possible.
                                // Good values are between [5.0, 10.0]. Smaller is better,
                                // but increases the likelihood of choppy sound.
    int mDelay;                 // Used to control the fps of the system. Values are 0 or 1.
};


#endif /* STREAMINGAUDIOQUEUE_H */