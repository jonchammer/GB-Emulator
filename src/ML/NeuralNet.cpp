/*
 * neural_net.cpp
 *
 *  Created on: Sep 8, 2014
 *      Author: Jon C. Hammer
 */

#include <cstdio>
#include <limits>
#include <random>
#include <ctime>
#include "ML/NeuralNet.h"

// Easier way to represent iterator of vector of doubles
typedef vector<double>::const_iterator vec_it;

namespace
{
    // Shuffles the elements of the given array
    void shuffle(vector<int>& vec, std::default_random_engine& generator, std::uniform_real_distribution<double>& rand)
    {
        if (vec.size() <= 1) return;

        for (int i = vec.size() - 1; i > 0; --i)
            std::swap(vec[i], vec[i * rand(generator)]);
    }

    // Fill in the given vector. [0, 1, 2, ... N - 1]
    void createElementVector(vector<int>& vec, const int N)
    {
        vec.resize(N);
        for (int i = 0; i < N; ++i)
            vec[i] = i;
    }
    
    void print(const vector<double>& vec)
    {
        cout << "[";
        for (size_t i = 0; i < vec.size(); ++i)
            cout << vec[i] << ", ";
        cout << "]" << endl;
    }
        
    // Implementation of error_function that prints the SSE/MSE to the screen when called.
    void sseErrorFunc(NeuralNet& net, int epoch, const Matrix& testFeatures, const Matrix& testLabels)
    {
        static time_t start = time(NULL);
        static time_t then  = start;
        double SSE          = net.measureSSE(testFeatures, testLabels);
        
        time_t now = (time(NULL) - start);

        // Print the header the first time through
        if (epoch == 0)
        {
            printf("%6s %10s %10s %10s %15s\n", "Epoch", "SSE", "MSE", "Time (s)", "Difference (s)");
            printf("%6d %10.2f %10.2f %10.2f\n", epoch, SSE, (SSE/testFeatures.rows()), difftime(now, 0));
        }

        else printf("%6d %10.2f %10.2f %10.2f %15.2f\n", epoch, SSE, (SSE/testFeatures.rows()), difftime(now, 0), difftime(now, then));

        cout.flush();
        then = now;
    }
    
    // Implementation of error_function that prints the number of misclassifications
    // and the misclassification rate to the screen when called.
    void categoricalErrorFunc(NeuralNet& net, int epoch, const Matrix& testFeatures, const Matrix& testLabels)
    {
        static time_t start = time(NULL);
        static time_t then  = start;
        double misses       = net.measureMisclassifications(testFeatures, testLabels);
        
        time_t now = time(NULL) - start;

        // Print the header the first time through
        if (epoch == 0)
        {
            printf("%6s %10s %10s %10s %15s\n", "Epoch", "Misses", "% Error", "Time (s)", "Difference (s)");
            printf("%6d %10.0f %10.2f %10.2f\n", epoch, misses, (100.0 * misses/testFeatures.rows()), difftime(now, 0));
        }

        else printf("%6d %10.0f %10.2f %10.2f %15.2f\n", epoch, misses, (100.0 * misses/testFeatures.rows()), difftime(now, 0), difftime(now, then));

        cout.flush();
        then = now;
    }
}

NeuralNet::NeuralNet(const vector<int>& layerDimensions)
{
	// Sanity check
	if (layerDimensions.size() < 2)
		throw Ex("At least 2 dimensions must be provided.");

	// Create each of the layers
	for (size_t i = 1; i < layerDimensions.size(); ++i)
	{
		Layer l(layerDimensions[i - 1], layerDimensions[i]);
		mLayers.push_back(l);
	}

	// Initialize the member variables
	mLearningRate     = DEFAULT_LEARNING_RATE;
	mMaxEpochs        = DEFAULT_MAX_EPOCHS;
}

// Returns the SSE of the given network measured against the given dataset
double NeuralNet::measureSSE(const Matrix& features, const Matrix& labels)
{
    vector<double> prediction(labels.cols(), 0);

    double sse = 0.0;

    for (size_t i = 0; i < features.rows(); ++i)
    {
        predict(features[i], prediction);
        
        const vector<double>& row = labels[i];
        for (size_t j = 0; j < labels.cols(); ++j)
        {
            double d = row[j] - prediction[j];

            // For categorical columns, use Hamming distance instead
            if (d != 0.0 && labels.valueCount(j) > 0)
                d = 1.0;

            sse += (d * d);
        }
    }

    return sse;
}

// Returns the number of misclassificiations of the given network measured against the given dataset.
// NOTE: It is assumed that the labels are in a 1-hot representation, e.g.: [0, 0, 0, 0, 1, 0, 0, 0, 0].
// If they are not, the results of this function are undefined.
int NeuralNet::measureMisclassifications(const Matrix& features, const Matrix& labels)
{
    vector<double> prediction(labels.cols(), 0);
    int misclassifications = 0;

    for (size_t i = 0; i < features.rows(); ++i)
    {
        predict(features[i], prediction);

        // Determine the largest activation in the prediction
        int largest = 0;
        double max  = prediction[0];

        for (size_t j = 1; j < prediction.size(); ++j)
        {
            if (prediction[j] > max)
            {
                largest = j;
                max     = prediction[j];
            }
        }

        // If the max column from the prediction does not coincide with
        // the '1', we have a misclassification.
        if (labels[i][largest] != 1.0)
            misclassifications++;
    }

    return misclassifications;
}

void NeuralNet::updateWeights(const vector<double>& feature, const vector<double>& label)
{
	static vector<double> prediction;
	prediction.resize(label.size());

	// 1. Forward Propagation
	predict(feature, prediction);

	// 2. Backward Propagation & Gradient Descent
	mLayers.back().backprop(prediction, label, mLearningRate);

	// NOTE: mLayers.size() returns size_t (unsigned). Answer
	// won't ever be negative, so we have to explicitly cast it
	// to an int.
	for (int i = ((int) mLayers.size()) - 2; i >= 0; --i)
		mLayers[i].backprop(mLayers[i + 1], mLearningRate);

	// 3. Update the weights in the first layer
	mLayers[0].backprop(feature, mLearningRate);
}

void NeuralNet::train(const Matrix& features, const Matrix& labels, error_function errorFunc,
    const Matrix* testFeatures, const Matrix* testLabels)
{
    Matrix tFeatures = (testFeatures == NULL) ? features : *testFeatures;
    Matrix tLabels   = (testLabels   == NULL) ? labels   : *testLabels;
    
    // Create the element order vector [0, 1, 2, 3, ... N-1]
    vector<int> elementOrder;
    createElementVector(elementOrder, features.rows());

    std::default_random_engine generator;
    std::uniform_real_distribution<double> rand(0.0, 1.0);
    int epoch = 0;
    
    if (errorFunc != NULL) (*errorFunc)(*this, epoch, tFeatures, tLabels);
    
    epoch++;
    while (epoch < mMaxEpochs)
    {
        // Complete one epoch
        shuffle(elementOrder, generator, rand);
        for (size_t j = 0; j < features.rows(); ++j)
        {
            int index = elementOrder[j];
            updateWeights(features[index], labels[index]);
        }

        // Report progress
        if (errorFunc != NULL) (*errorFunc)(*this, epoch, tFeatures, tLabels);
        epoch++;
    }
}

void NeuralNet::trainSSE(const Matrix& features, const Matrix& labels, const Matrix* testFeatures, const Matrix* testLabels)
{
    train(features, labels, &sseErrorFunc, testFeatures, testLabels);
}

void NeuralNet::trainCategorical(const Matrix& features, const Matrix& labels, const Matrix* testFeatures, const Matrix* testLabels)
{
    train(features, labels, &categoricalErrorFunc, testFeatures, testLabels);
}

void NeuralNet::setParams(const vector<double>& params)
{
	size_t wbIndex = 0;

	for (size_t i = 0; i < mLayers.size(); ++i)
	{
		// Assign weights
		Matrix& weights = mLayers[i].getWeights();

		for (size_t j = 0; j < weights.rows(); ++j)
		{
			vec_it start = params.begin() + wbIndex;
			vec_it stop  = start + weights.cols();
			std::copy(start, stop, weights[j].begin());

			wbIndex += weights.cols();
		}

		// Assign biases
		vector<double>& out = mLayers[i].getBias();
		vec_it start = params.begin() + wbIndex;
		vec_it stop  = start + out.size();
		std::copy(start, stop, out.begin());

		wbIndex += out.size();
	}
}

void NeuralNet::getParams(vector<double>& params)
{
    size_t wbIndex = 0;

	for (size_t i = 0; i < mLayers.size(); ++i)
	{
        // Make sure we have enough room to store everything
        params.resize(wbIndex + mLayers[i].getOutputSize() + 
            mLayers[i].getInputSize()* mLayers[i].getOutputSize() );
        
		// Save weights
		Matrix& weights = mLayers[i].getWeights();
        
		for (size_t j = 0; j < weights.rows(); ++j)
		{
            std::copy(weights[j].begin(), weights[j].end(), params.begin() + wbIndex);            
			wbIndex += weights.cols();
		}

		// Save biases
		vector<double>& out = mLayers[i].getBias();
        std::copy(out.begin(), out.end(), params.begin() + wbIndex);
		wbIndex += out.size();
	}
}

void NeuralNet::predict(const vector<double>& in, vector<double>& out)
{
	mLayers[0].feed(in);

	// Feed the activation from the previous layer into the current layer.
	for (size_t i = 1; i < mLayers.size(); ++i)
		mLayers[i].feed(mLayers[i - 1].getActivation());

	// Copy the activation from the last layer into the output
	vector<double>& act = mLayers[mLayers.size() - 1].getActivation();
	out.resize(act.size());

	// Copy the activation into the output
	std::copy(act.begin(), act.end(), out.begin());
}
