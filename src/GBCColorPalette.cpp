#include "GBCColorPalette.h"

namespace
{
    // The Neural Net used to create the mapping between the individual rgb intensities
    // and the final color is small enough that the weights can be stored in the program.
    // These represent a pretrained network with a 3 x 20 x 3 topology that maps from
    // an intensity [0.0, 1.0] for each RGB to a final color [0.0, 1.0] for each of the
    // three output channels. Inputs from the GBC are usually [0, 31], so they must be
    // divided by 31.0 first. Outputs need to be multiplied by 255.0 to be used by the
    // rendering hardware.
    const vector<double> NN_WEIGHTS = 
    {
         0.3726700, -0.4625980,  0.3469020, -0.00495657,  0.200679000,  0.0376900, -0.28477100, 
        -0.2524180,  0.8480880,  0.3144320,  0.17491000, -0.162717000,  0.0701903,  0.53090800, 
         0.0500920, -2.2735300, -1.4986900, -1.96246000,  0.049032700,  0.0112270,  0.06147100, 
         0.3221670,  0.8932520,  0.1734960,  0.17182300,  0.018715800, -0.2640860,  0.07198640, 
        -0.0403830, -0.1946450, -0.4224200,  0.13582300, -0.018215300,  1.7411300, -0.16733300, 
        -0.0665957,  0.1872900, -0.5305360, -0.56041100,  0.772292000, -1.0488800, -0.92197500, 
         0.0388172,  0.0258715,  1.5924300,  0.48376000, -0.000803654,  0.3955870,  0.37363300, 
         0.3349310, -0.4082810,  0.6609380, -0.17875200, -0.035746200,  0.1015880, -0.53066600, 
         0.2934230, -0.1333970, -0.0233648, -0.03558940, -0.326842000, -0.2380940,  0.01721010, 
        -0.2163240, -0.9176090, -0.9010830, -0.31999400, -1.063090000,  0.1348080,  0.41650300,
        -0.0347036, -1.0676400, -0.2611750,  0.38263400, -1.207450000, -0.2254310, -0.22145000, 
        -0.6119280,  0.0824766,  0.3147730, -0.09219780, -0.148894000,  0.1509690,  0.09915830, 
        -0.1387370, -0.4702630, -0.2062020,  0.11998700,  0.177092000,  0.2735600, -0.00818466, 
         1.0263200, -0.2595140,  0.0130191,  0.17410400, -0.027169800,  0.2234810,  0.33474300, 
        -0.1379230,  0.0381076, -0.5432540,  0.04049490,  0.409085000, -0.0199066,  0.29303900, 
        -0.9995040, -0.1190740,  0.7258860,  0.19163800,  0.136514000, -0.0168524,  0.62036500, 
         0.3385030, -0.2673900,  0.3321940, -0.37020300, -0.136063000, -0.5547950, -0.29621200, 
         0.0538045, -0.1364830, -0.0507380,  0.32001100, -0.118305000, -0.1278310, -0.75974000, 
        -0.1982150,  0.2205010, -0.0117423,  0.13959700,  0.168643000,  0.4155210, -0.01673080, 
        -0.0748613,  1.1461900, -0.1909780, -0.09764930, -0.290595000,  0.0941684,  0.16039400, 
         0.5137940,  0.3052090,  0.3558490
    };

    void trainNetwork(NeuralNet& nn, const string& datasetFilename)
    {
        nn.setLearningRate(0.01);
        nn.setMaxEpochs(30000);

        // Load and normalize the data
        Matrix allData;
        allData.loadARFF(datasetFilename);

        for (size_t j = 0; j < allData.rows(); ++j)
        {
            allData[j][0] /= 31;
            allData[j][1] /= 31;
            allData[j][2] /= 31;
            allData[j][3] /= 255.0;
            allData[j][4] /= 255.0;
            allData[j][5] /= 255.0;
        }

        Matrix features, labels;
        features.copyPart(allData, 0, 0, allData.rows(), 3);
        labels.copyPart(allData, 0, 3, allData.rows(), 3);

        cout << "Training network..." << endl;
        nn.trainSSE(features, labels);
        cout << "SSE: " << nn.measureSSE(features, labels) << endl;

        // Get the network parameters and print them to the screen. (We can copy
        // and paste into NN_WEIGHTS to update the network if needed.)
        vector<double> params;
        nn.getParams(params);

        for (size_t i = 0; i < params.size(); ++i)
        {
            cout << params[i] << ", ";
            if (i != 0 && i % 7 == 0)
                cout << endl;
        }
    }

    int calculateColor(NeuralNet& nn, int index)
    {
        byte r = ( index        & 0x1F);
        byte g = ((index >>  5) & 0x1F);
        byte b = ((index >> 10) & 0x1F);

        static vector<double> in(3);
        static vector<double> out(3);

        // Get the red, green, and blue bases
        in[0] = r / 31.0;
        in[1] = g / 31.0;
        in[2] = b / 31.0;

        // Use the NN to calculate the corresponding GBC color
        nn.predict(in, out);

        if (out[0] < 0.0) out[0] = 0.0;
        if (out[1] < 0.0) out[1] = 0.0;
        if (out[2] < 0.0) out[2] = 0.0;

        r = (byte) (out[0] * 255);
        g = (byte) (out[1] * 255);
        b = (byte) (out[2] * 255);

        return (0xFF << 24) | (b << 16) | (g << 8) | r;
    }
    
    void RGBToHSL(byte red, byte green, byte blue, double& h, double& s, double& l)
    {
        double r = red   / 255.0;
        double g = green / 255.0;
        double b = blue  / 255.0;

        double r2, g2, b2;

        h = 0.0, s = 0.0, l = 0.0; // default to black
        double maxRGB = max(max(r, g), b);
        double minRGB = min(min(r, g), b);

        l = (minRGB + maxRGB) / 2.0;
        if (l <= 0.0) return;

        double vm = maxRGB - minRGB;
        s = vm;
        if (s <= 0.0) return;

        s /= ((l <= 0.5) ? (maxRGB + minRGB ) : (2.0 - maxRGB - minRGB));

        r2 = (maxRGB - r) / vm;
        g2 = (maxRGB - g) / vm;
        b2 = (maxRGB - b) / vm;

        if (r == maxRGB)
              h = (g == minRGB ? 5.0 + b2 : 1.0 - g2);
        else if (g == maxRGB)
              h = (b == minRGB ? 1.0 + r2 : 3.0 - b2);
        else
              h = (r == minRGB ? 3.0 + g2 : 5.0 - r2);

        h /= 6.0;
    }

    void HSLToRGB(double h, double s, double l, byte& red, byte& green, byte& blue)
    {
        double r = l, g = l, b = l;   // default to gray
        double v = ((l <= 0.5) ? (l * (1.0 + s)) : (l + s - l * s));

        if (v > 0)
        {
            double m  = l + l - v;
            double sv = (v - m ) / v;

            h *= 6.0;

            int sextant  = (int)h;
            double fract = h - sextant;
            double vsf   = v * sv * fract;
            double mid1  = m + vsf;
            double mid2  = v - vsf;

            switch (sextant)
            {
                case 0:
                    r = v;
                    g = mid1;
                    b = m;
                    break;
                case 1:
                    r = mid2;
                    g = v;
                    b = m;
                    break;
                case 2:
                    r = m;
                    g = v;
                    b = mid1;
                    break;
                case 3:
                    r = m;
                    g = mid2;
                    b = v;
                    break;
                case 4:
                    r = mid1;
                    g = m;
                    b = v;
                    break;
                case 5:
                    r = v;
                    g = m;
                    b = mid2;
                    break;
              }
        }

        // Convert from [0, 1] to [0, 255]
        red   = r * 255;
        green = g * 255;
        blue  = b * 255;
    }
};

GBCColorPalette::GBCColorPalette()
{
    mColors = new int[PALETTE_SIZE];
    reset();
}

GBCColorPalette::~GBCColorPalette()
{
    delete[] mColors;
    mColors = NULL;
}

bool GBCColorPalette::loadColorsFromFile(const string& dataFilename)
{
    // Open the data file
    ifstream din;
    din.open(dataFilename.c_str(), ios::binary | ios::ate);
    if (!din) return false;
    
    // Read the length of the file first
    int length = din.tellg() / sizeof(int);
    din.seekg(0, ios::beg);
    
    // If the file length is too small, something went wrong
    if (length < PALETTE_SIZE)
    {
        din.close();
        return false;
    }
    
    // Read the data all at once
    din.read((char*) mColors, sizeof(int) * std::min(length, PALETTE_SIZE));
    din.close();
    return true;
}

bool GBCColorPalette::saveColorsToFile(const string& filename)
{
    ofstream dout;
    dout.open(filename.c_str(), ios::out | ios::binary);
    if (!dout) return false;
    
    dout.write((char*) mColors, sizeof(int) * PALETTE_SIZE);
    dout.close();
    return true;
}

void GBCColorPalette::generateColors()
{
    // Set up the network
    vector<int> dimensions = {3, 20, 3};
    NeuralNet nn(dimensions);
    
    // Load the weights
    nn.setParams(NN_WEIGHTS);
    
    // Or train the network - Uncomment this line to start the training
    // from scratch. Make sure the training file is available, though.
    //trainNetwork(nn, "../gbc_color_analysis.arff");
    
    for (int i = 0; i < PALETTE_SIZE; ++i)
        mColors[i] = calculateColor(nn, i);
    
     // Set a few key colors manually in case the NN has a "bad day".
    mColors[0x0000] = 0xFF000000;
    mColors[0x7FFF] = 0xFFFFFFFF;
}

void GBCColorPalette::reset()
{
    // Try to load the colors from a file if possible
    if (!loadColorsFromFile("gbc_colors.dat"))
    {
        cout << "Unable to load GBC Color Palette. Regenerating..." << endl;
        
        // Fall back to the NN and recreate the data if necessary
        generateColors();
        
        // Save the data to the file so we won't need to go through this
        // step next time
        if (saveColorsToFile("gbc_colors.dat"))
            cout << "Successfully saved GBC Color Palette" << endl;
        else cout << "Unable to save GBC Color Palette" << endl;
    }
    else cout << "Successfully loaded GBC Color Palette" << endl;
}

void GBCColorPalette::setGamma(double gamma)
{
    // A gamma of 1.0 means there is no change
    if (gamma == 1.0) return;
    
    double h, s, l;
    for (int i = 0; i < PALETTE_SIZE; ++i)
    {
        byte b = (mColors[i]      ) & 0xFF;
		byte g = (mColors[i] >> 8 ) & 0xFF;
		byte r = (mColors[i] >> 16) & 0xFF;
                
        // Convert the color to HSL so we can manipulate it easier
        RGBToHSL(r, g, b, h, s, l);
        
        // Apply gamma correction
        l = pow(l, gamma);
        
        // Convert the color back to RGB
        HSLToRGB(h, s, l, r, g, b);
		mColors[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
}

void GBCColorPalette::setSaturation(double saturation)
{
    // A saturation of 1.0 means there is no change
    if (saturation == 1.0) return;
    
    double h, s, l;
    for (int i = 0; i < PALETTE_SIZE; ++i)
    {
        byte b = (mColors[i]      ) & 0xFF;
		byte g = (mColors[i] >> 8 ) & 0xFF;
		byte r = (mColors[i] >> 16) & 0xFF;
                
        // Convert the color to HSL so we can manipulate it easier
        RGBToHSL(r, g, b, h, s, l);
        
        // Change the saturation level
        s *= saturation;
        
        // Convert the color back to RGB
        HSLToRGB(h, s, l, r, g, b);
		mColors[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
}