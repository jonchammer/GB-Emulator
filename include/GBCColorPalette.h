/* 
 * File:   GBCColorConverter.h
 * Author: Jon C. Hammer
 *
 * Created on July 3, 2016, 12:54 PM
 */

#ifndef GBCCOLORCONVERTER_H
#define GBCCOLORCONVERTER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include "Common.h"
#include "ML/NeuralNet.h"

constexpr int PALETTE_SIZE = 32768;

class GBCColorPalette
{
public:
    GBCColorPalette();
    virtual ~GBCColorPalette();
    
    int getColor(int index) { return mColors[index]; } 
    
    /**
     * Reset the colors to their default values.
     */
    void reset();
    
    /**
     * Set the gamma level for the colors. gamma = 1.0 is no change,
     * gamma > 1.0 will darken the image, and gamma < 1.0 will brighten it.
     */
    void setGamma(double gamma);
    
    /**
     * Change the saturation level for the colors. saturation = 1.0 is no
     * change, and saturation = 0.0 is black and white.
     */
    void setSaturation(double saturation);
    
private:
    int* mColors;
    
    bool loadColorsFromFile(const string& dataFilename);
    bool saveColorsToFile(const string& filename);
    void generateColors();
};

#endif /* GBCCOLORCONVERTER_H */

