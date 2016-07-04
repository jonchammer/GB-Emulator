/*
 * layer.cpp
 *
 *  Created on: Sep 8, 2014
 *      Author: Jon C. Hammer
 */

#include <cmath>
#include <random>
#include "ML/layer.h"

void mult_mat_vec(const Matrix& m, const std::vector<double>& vec, std::vector<double>& out)
{
    out.resize(m.rows());

    for (size_t i = 0; i < m.rows(); ++i)
    {
        double sum = 0.0;
        for (size_t j = 0; j < m.cols(); ++j)
            sum += m[i][j] * vec[j];

        out[i] = sum;
    }
}

Layer::Layer(size_t inputs, size_t outputs)
{
	resize(inputs, outputs);
}

void Layer::resize(size_t inputs, size_t outputs)
{
	// Initialize the member variables
	mWeights.setSize(outputs, inputs);
	mBias.resize(outputs);
	mBlame.resize(outputs);
	mActivation.resize(outputs);

	// Initialize the weights and biases to small random values
    std::default_random_engine generator;
	std::normal_distribution<> rand(0.0, 1.0);

	double max = fmax(0.03, 1.0 / (double) inputs);
	for (size_t i = 0; i < outputs; ++i)
	{
		for (size_t j = 0; j < inputs; ++j)
			mWeights[i][j] = max * rand(generator);
			
		mBias[i] = max * rand(generator);
	}
}

void Layer::feed(const vector<double>& x)
{
	// Compute W * x
	static vector<double> temp(mWeights.rows());
	mult_mat_vec(mWeights, x, temp);

	// Compute the activation function
	for (size_t i = 0; i < mActivation.size(); ++i)
		mActivation[i] = tanh(temp[i] + mBias[i]);
}

// Calculates the blame terms for the output layer
void Layer::backprop(const vector<double>& prediction, const vector<double>& truth, const double learningRate)
{
	for (size_t k = 0; k < truth.size(); ++k)
	{
		double c  = mActivation[k];
		mBlame[k] = (truth[k] - prediction[k]) * (1.0 - (c * c));

		// Adjust the biases using gradient descent
		mBias[k] += learningRate * mBlame[k];
	}
}

// Calculates the blame terms for the input layer and each hidden layer
void Layer::backprop(Layer& nextLayer, const double learningRate)
{
	for (int i = 0; i < nextLayer.getInputSize(); ++i)
	{
		double sum  = 0.0;

		// Because we're using tanh(x) as our activation function and the
		// derivative can be expressed in terms of tanh(x), we don't need
		// to calculate tanh(x) twice. mActivation[i] = tanh(mP[i]) = c
		double c    = mActivation[i];
		double actD = 1.0 - c * c;

		for (int j = 0; j < nextLayer.getOutputSize(); ++j)
		{
			sum += nextLayer.mWeights[j][i] * nextLayer.mBlame[j] * actD;

			// Adjust the weights using gradient descent
			nextLayer.mWeights[j][i] += learningRate * nextLayer.mBlame[j] * c;
		}

		mBlame[i] = sum;

		// Adjust the biases using gradient descent
		mBias[i] += learningRate * mBlame[i];
	}
}

// Performs the final weight update for the input layer
void Layer::backprop(const vector<double>& feature, const double learningRate)
{
	for (int i = 0; i < getInputSize(); ++i)
	for (int j = 0; j < getOutputSize(); ++j)
		mWeights[j][i] += learningRate * mBlame[j] * feature[i];
}
