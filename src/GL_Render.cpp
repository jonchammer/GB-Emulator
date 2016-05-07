/* 
 * File:   Render.cpp
 * Author: Jon C. Hammer
 *
 * Created on April 15, 2016, 7:47 PM
 */

#include <GL/glew.h>
#include <GL/glut.h>
#include "Common.h"

// Globals
int windowWidth  = 640;  // Width of the current window in pixels
int windowHeight = 480;  // Height of the current window in pixels

Emulator* em;           // The emulator that is being used
string title;           // The title of the game (used as the title of the window))
GLuint texID;           // The ID of the texture that contains the screen content

// FPS calculation
void updateFPS()
{
    static int frame = 0; 
    static int time;
    static int timebase = 0;
    static char titleBuffer[100];
    
    frame++;
    time = glutGet(GLUT_ELAPSED_TIME);
    if (time - timebase > 1000)
    {
        double fps = frame * 1000.0 / (time - timebase);
        timebase   = time;
        frame      = 0;
        
        // Update the title so we can see the FPS
        sprintf(titleBuffer, "%s. FPS = %.2f", title.c_str(), fps);
        glutSetWindowTitle(titleBuffer);
        
        // Save the game if the game has asked for it
        em->getMemory()->updateSave();
    }
}

void render()
{
    updateFPS();
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Pull the most recent screen buffer from the emulator and use it
    // to fill the texture
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS, 
        GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*) em->getGraphics()->getScreenData());
    
    // Draw the texture to the screen
    glBegin(GL_QUADS);
    
        glTexCoord2f(0, 0);  glVertex2f(10,           10);
        glTexCoord2f(1, 0);  glVertex2f(windowWidth - 10, 10);
        glTexCoord2f(1, 1);  glVertex2f(windowWidth - 10, windowHeight - 10);
        glTexCoord2f(0, 1);  glVertex2f(10,           windowHeight - 10);
    
    glEnd();
   
    glutSwapBuffers();
}

// Update function
void update(int)
{
    int startTime = glutGet(GLUT_ELAPSED_TIME);
    
    // Update emulator
    em->update();
        
    // Render
    glutPostRedisplay();
    
    // Reset timer. 60 fps == 17 milliseconds per update. Subtract out the
    // time spent doing the update so we average about 60 fps constantly.
    // Add 1 so we're more likely to be over 60 than under.
    int diff = glutGet(GLUT_ELAPSED_TIME) - startTime + 1;
    glutTimerFunc(17 - diff, update, 0);
}

// Window reshaping function
void reshape(GLsizei w, GLsizei h)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, h, 0);
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, w, h);
    
    // Resize quad
    windowWidth  = w;
    windowHeight = h;
}

// Keyboard inputs
void keyDown(byte key, int x, int y)
{
    switch (key)
    {
        case 32: em->getInput()->keyPressed(BUTTON_SELECT); break; // Space bar
        case 13: em->getInput()->keyPressed(BUTTON_START);  break; // Enter
        case 's': case 'S': em->getInput()->keyPressed(BUTTON_A); break;
        case 'a': case 'A': em->getInput()->keyPressed(BUTTON_B); break;
        
        case 'r': case 'R': 
        {
            em->reset(); 
            em->getMemory()->loadCartridge("Tetris.gb"); 
            break;
        }
        
        //case 'p': case 'P':
        //{
        //    em->getMemory()->saveGame("roms/Pokemon_Red.sav");
        //}
        case 'l': case 'L':
        {
            em->getCPU()->setLogging(true);
        }
    }
}

void keyUp(byte key, int x, int y)
{
    switch (key)
    {
        case 32: em->getInput()->keyReleased(BUTTON_SELECT); break; // Space bar
        case 13: em->getInput()->keyReleased(BUTTON_START);  break; // Enter
        case 's': case 'S': em->getInput()->keyReleased(BUTTON_A); break;
        case 'a': case 'A': em->getInput()->keyReleased(BUTTON_B); break;
    } 
}

void specialDown(int key, int x, int y)
{
    switch (key)
    {
        case GLUT_KEY_UP:    em->getInput()->keyPressed(BUTTON_UP);    break;
        case GLUT_KEY_DOWN:  em->getInput()->keyPressed(BUTTON_DOWN);  break;
        case GLUT_KEY_LEFT:  em->getInput()->keyPressed(BUTTON_LEFT);  break;
        case GLUT_KEY_RIGHT: em->getInput()->keyPressed(BUTTON_RIGHT); break;
    }
}

void specialUp(int key, int x, int y)
{
    switch (key)
    {
        case GLUT_KEY_UP:    em->getInput()->keyReleased(BUTTON_UP);    break;
        case GLUT_KEY_DOWN:  em->getInput()->keyReleased(BUTTON_DOWN);  break;
        case GLUT_KEY_LEFT:  em->getInput()->keyReleased(BUTTON_LEFT);  break;
        case GLUT_KEY_RIGHT: em->getInput()->keyReleased(BUTTON_RIGHT); break;
    } 
}

void initializeRenderer(int argc, char** argv, Emulator* emulator)
{
    em    = emulator;
    title = em->getMemory()->getCartridgeName();
    
    // Initialize GLUT and the window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - windowWidth)/2, (glutGet(GLUT_SCREEN_HEIGHT) - windowHeight) / 2);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow(title.c_str());
    
    // Create the texture we will render into
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);
    
    // Set the callbacks
    glutDisplayFunc(render);
    glutReshapeFunc(reshape);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutTimerFunc(17, update, 0);
    
    // Get started
    glutMainLoop();
}