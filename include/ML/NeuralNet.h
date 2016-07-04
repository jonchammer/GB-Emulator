/*
 * neural_net.h
 *
 *  Created on: Sep 8, 2014
 *      Author: Jon C. Hammer
 */

#ifndef NEURAL_NET_H
#define NEURAL_NET_H

#include <cstring>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "Error.h"
#include "Matrix.h"
#include "Layer.h"

using std::vector;
using namespace ml;

class NeuralNet;

namespace
{
    // Easier way to represent iterator of vector of doubles
    typedef vector<double>::const_iterator vec_it;
    typedef void (*error_function)(NeuralNet&, int, const Matrix&, const Matrix&);
}

// Represents a multi-layer perceptron
class NeuralNet
{
public:

    // --------------------- Constructors / Destructors --------------------- //
    NeuralNet(const vector<int>& layerDimensions);
    virtual ~NeuralNet() {};

    // --------------------- Methods --------------------- //

    /// Trains this network on the given training data (the first two matrices).
    /// The given error function will be called at each iteration. This should be 
    /// used to keep track of network training progress (e.g. displaying output to the user).
    /// The last two arguments, if provided, specify the data to be used for testing.
    /// If they are NULL, the training feature and label matrices will be used instead.
    void train(const Matrix& features, const Matrix& labels, error_function errorFunc,
        const Matrix* testFeatures = NULL, const Matrix* testLabels = NULL);
    
    /// Trains this network on the given training data (the first two matrices).
    /// An error function that reports the SSE/MSE will be used.
    /// The last two arguments, if provided, specify the data to be used for testing.
    /// If they are NULL, the training feature and label matrices will be used instead.
    virtual void trainSSE(const Matrix& features, const Matrix& labels, const Matrix* testFeatures = NULL, const Matrix* testLabels = NULL);
    
    /// Trains this network on the given training data (the first two matrices).
    /// An error function that reports the number of misclassifications and the
    /// misclassification rate will be used.
    /// The last two arguments, if provided, specify the data to be used for testing.
    /// If they are NULL, the training feature and label matrices will be used instead.
    virtual void trainCategorical(const Matrix& features, const Matrix& labels, const Matrix* testFeatures = NULL, const Matrix* testLabels = NULL);
    
    /// Makes a prediction using the current weights of the network.
    /// 'in' contains the feature vector. 'out' is where the results will be placed.
    void predict(const vector<double>& in, vector<double>& out);
    
    double measureSSE(const Matrix& features, const Matrix& labels);
    int measureMisclassifications(const Matrix& features, const Matrix& labels);
    
    // --------------------- Getters & Setters --------------------- //

    Layer& getLayer(int i)
    {
        return mLayers[i];
    }

    double getLearningRate()
    {
        return mLearningRate;
    }

    void setMaxEpochs(int max)
    {
        mMaxEpochs = max;
    }

    void setLearningRate(double rate)
    {
        mLearningRate = rate;
    }

    // Sets the weights and biases of every layer in the network
    void setParams(const vector<double>& params);
    void getParams(vector<double>& params);
    
private:

    // --------------------- Constants --------------------- //
    constexpr static double DEFAULT_LEARNING_RATE = 0.01;
    constexpr static int DEFAULT_MAX_EPOCHS       = 30;

    // --------------------- Methods --------------------- //

    // Updates the weights using backprop and gradient descent one instance at a time
    // @param feature - The features of the instance
    // @param label   - The labels of the instance
    void updateWeights(const vector<double>& feature, const vector<double>& label);

    // --------------------- Member Variables --------------------- //
    vector<Layer> mLayers; // Holds the actual layers of the MLP
    double mLearningRate;  // Controls the speed of stochastic gradient descent
    int mMaxEpochs;        // The maximum number of epochs that can be performed
};

#endif /* NEURAL_NET_H */
