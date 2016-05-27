/* 
 * File:   Render.cpp
 * Author: Jon C. Hammer
 *
 * Created on April 15, 2016, 7:47 PM
 */

#include "Common.h"
#include "SDL.h"
#include "Sound/StreamingAudioQueue.h"

const int DEFAULT_WINDOW_WIDTH  = SCREEN_WIDTH_PIXELS  * 4;
const int DEFAULT_WINDOW_HEIGHT = SCREEN_HEIGHT_PIXELS * 4;

// Globals
Emulator* em;  // The emulator that is being used
string title;  // The title of the game (used as the title of the window))

// Returns true when the user has chosen to exit the program
bool handleEvents()
{
    static SDL_Event event;
    
    while (SDL_PollEvent(&event) != 0)
    {
        switch (event.type)
        {
            // Exit the main loop
            case SDL_QUIT: return true;
            
            // Handle key down events
            case SDL_KEYDOWN:
            {
                switch (event.key.keysym.sym)
                {
                    // Standard controls
                    case SDLK_UP:     em->getInput()->keyPressed(BUTTON_UP);     break;
                    case SDLK_DOWN:   em->getInput()->keyPressed(BUTTON_DOWN);   break;
                    case SDLK_LEFT:   em->getInput()->keyPressed(BUTTON_LEFT);   break;
                    case SDLK_RIGHT:  em->getInput()->keyPressed(BUTTON_RIGHT);  break;
                    case SDLK_SPACE:  em->getInput()->keyPressed(BUTTON_SELECT); break;
                    case SDLK_RETURN: em->getInput()->keyPressed(BUTTON_START);  break;
                    case SDLK_s:      em->getInput()->keyPressed(BUTTON_A);      break;
                    case SDLK_a:      em->getInput()->keyPressed(BUTTON_B);      break;
                    
                    // Sound channels
                    case SDLK_F1: em->getSound()->toggleSound1(); break;
                    case SDLK_F2: em->getSound()->toggleSound2(); break;
                    case SDLK_F3: em->getSound()->toggleSound3(); break;
                    case SDLK_F4: em->getSound()->toggleSound4(); break;
                    
                    // Misc.
                    case SDLK_p: em->togglePaused(); break;
                    //case SDLK_g: em->getGraphics()->toggleGrid(); break;
                }
                break;
            }
            
            // Handle key up events
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym)
                {
                    case SDLK_UP:     em->getInput()->keyReleased(BUTTON_UP);     break;
                    case SDLK_DOWN:   em->getInput()->keyReleased(BUTTON_DOWN);   break;
                    case SDLK_LEFT:   em->getInput()->keyReleased(BUTTON_LEFT);   break;
                    case SDLK_RIGHT:  em->getInput()->keyReleased(BUTTON_RIGHT);  break;
                    case SDLK_SPACE:  em->getInput()->keyReleased(BUTTON_SELECT); break;
                    case SDLK_RETURN: em->getInput()->keyReleased(BUTTON_START);  break;
                    case SDLK_s:      em->getInput()->keyReleased(BUTTON_A);      break;
                    case SDLK_a:      em->getInput()->keyReleased(BUTTON_B);      break;
                }
                
                break;
            }
        }
    }
    
    return false;
}

// FPS calculation - updates the window title once a second.
void updateFPS(SDL_Window* window)
{
    static int frame = 0; 
    static int time;
    static int timebase = 0;
    static char titleBuffer[100];
    
    frame++;
    time = SDL_GetTicks();
    if (time - timebase > 1000)
    {
        double fps = frame * 1000.0 / (time - timebase);
        timebase   = time;
        frame      = 0;
        
        // Update the title so we can see the FPS
        snprintf(titleBuffer, 100, "%s. FPS = %.2f", title.c_str(), fps);
        SDL_SetWindowTitle(window, titleBuffer);
        
        // Save the game if the game has asked for it
        em->getMemory()->getLoadedCartridge()->save();
    }
}

// Returns true if the user has chosen to exit the program.
bool update(SDL_Window* window, SDL_Texture* buffer, SDL_Renderer* renderer, StreamingAudioQueue* audioQueue)
{
    static int* pixels;
    static int pitch;
    
    int startTime = SDL_GetTicks();
    updateFPS(window);

    // Take care of input and other events
    if (handleEvents())
        return true;

    // Update the emulator
    em->update();

    // Update the SDL texture with the most recent pixel data
    SDL_LockTexture(buffer, NULL, (void**) &pixels, &pitch);   
    memcpy(pixels, em->getGraphics()->getScreenData(), pitch * SCREEN_HEIGHT_PIXELS);
    SDL_UnlockTexture(buffer);

    // Render the texture to the screen
    SDL_RenderCopy(renderer, buffer, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    // In order to update the audio as fast as it is being handled, we
    // add an additional delay. When this is high, we will update faster.
    // When it is low, we will update slower.
    int delay = audioQueue->getDelay();

    // Wait a bit to maintain the FPS.
    // 60 fps == 17 milliseconds per update. Subtract out the
    // time spent doing the update so we average about 60 fps constantly.
    // Also, make sure not to ask for a negative delay...
    int diff = 17 - (SDL_GetTicks() - startTime + delay);
    if (diff > 0) 
        SDL_Delay(diff);
    
    return false;
}

// This will be called by the emulator as soon as the internal sound buffer
// has been filled. We pass it on to the audio queue, which sends it to SDL.
void soundFunc(void* udata, short* buffer, int length)
{
    StreamingAudioQueue* queue = (StreamingAudioQueue*) udata;
    if (queue != NULL)
        queue->append(buffer, length);
}

/**
 * This function is defined in Emulator.h and called in the main function.
 * It is the interface between the host platform and the emulator.
 */
void startEmulation(int /*argc*/, char** /*argv*/, Emulator* emulator)
{
    em    = emulator;
    title = em->getMemory()->getLoadedCartridge()->getCartridgeInfo().name;
    
    // Initialize SDL    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        cout << "Unable to initialize SDL. SDL_Error: " << SDL_GetError() << endl;
        return;
    }
    
    // Initialize the Window
    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        cout << "Unable to create window. SDL_Error: " << SDL_GetError() << endl;
        return;
    }
    
    // Get the SDL constructs we need to render the game
    SDL_Renderer* renderer     = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* buffer        = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
        SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS);
   
    // Create the audio queue, and attach the 'soundFunc' callback so the emulator
    // knows what to do when it fills the sound buffer.
    StreamingAudioQueue* audioQueue = new StreamingAudioQueue(44100, 2048);
    em->getSound()->setSoundCallback(&soundFunc, audioQueue);
    
    // Main loop
    while (!update(window, buffer, renderer, audioQueue));

    // Cleanup
    delete audioQueue;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}