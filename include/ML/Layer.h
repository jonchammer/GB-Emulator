/*
 * layer.h
 *
 *  Created on: Sep 8, 2014
 *      Author: Jon C. Hammer
 */

#ifndef LAYER_H
#define LAYER_H

#include <cstring>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <cmath>
#include "Matrix.h"

using std::cout;
using std::endl;
using std::vector;
using namespace ml;

// Represents one layer of a multi-layer perceptron (Neural Net)

class Layer
{
public:

    // --------------------- Constructors / Destructors --------------------- //
    Layer(size_t inputs, size_t outputs);
    virtual ~Layer() {};

    // --------------------- Methods --------------------- //

    // Change the size of this layer
    void resize(size_t inputs, size_t outputs);

    // Used for evaluating this layer. This updates the activation based on y = a(Wx + b)
    void feed(const vector<double>& x);

    // Backprop routines (for output layer, hidden layers, and input layer, respectively)
    void backprop(const vector<double>& prediction, const vector<double>& truth, const double learningRate);
    void backprop(Layer& nextLayer, const double learningRate);
    void backprop(const vector<double>& feature, const double learningRate);

    // --------------------- Getters & Setters --------------------- //

    Matrix& getWeights()
    {
        return mWeights;
    }

    vector<double>& getBias()
    {
        return mBias;
    }

    vector<double>& getBlame()
    {
        return mBlame;
    }

    vector<double>& getActivation()
    {
        return mActivation;
    }

    int getInputSize()
    {
        return mWeights.cols();
    }

    int getOutputSize()
    {
        return mBias.size();
    }

private:

    // --------------------- Member Variables --------------------- //
    Matrix mWeights;            // The weights of the inputs and outputs. y = a(Wx + b)
    vector<double> mBias;       // The biases of the equation
    vector<double> mBlame;      // The errors that result from backprop
    vector<double> mActivation; // The activation (output of this layer)
};

#endif /* LAYER_H */
