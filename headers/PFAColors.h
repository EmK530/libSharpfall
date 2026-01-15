#pragma once

#include <vector>
#include <string>
#include "tinyxml2.h"

// Your ColorData struct (floats 0-1)
struct ColorData { float r, g, b, a; };

class PFAColors {
public:
    static bool Ready;
    static std::vector<ColorData> trackColors;
    static std::vector<ColorData> PFAConfig;

    // Initialize the colors with the given number of tracks
    static void Init(int tracks);

private:
    // Converts HSV to RGB (all floats 0-1)
    static ColorData HSVToRGB(float h, float s, float v);

    // Gets APPDATA path safely on Windows
    static std::string GetAppDataPath();
};
