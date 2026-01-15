#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include "tinyxml2.h"
#include "headers/PFAColors.h"

// Static member definitions
bool PFAColors::Ready = false;
std::vector<ColorData> PFAColors::trackColors;
std::vector<ColorData> PFAColors::PFAConfig;

// Private helper: HSV to RGB
ColorData PFAColors::HSVToRGB(float h, float s, float v) {
    float r, g, b;
    int i = int(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = g = b = 0.f; break;
    }

    return ColorData{ r, g, b, 1.0f };
}

// Private helper: get APPDATA path
std::string PFAColors::GetAppDataPath() {
    char* buffer = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buffer, &len, "APPDATA") == 0 && buffer != nullptr) {
        std::string appData(buffer);
        free(buffer);
        return appData;
    }
    return "";
}

// Public method: initialize colors
void PFAColors::Init(int tracks) {
    PFAConfig.clear();
    std::string path = GetAppDataPath() + "\\Piano From Above\\Config.xml";

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* root = doc.FirstChildElement("PianoFromAbove");
        if (root) {
            tinyxml2::XMLElement* visual = root->FirstChildElement("Visual");
            if (visual) {
                tinyxml2::XMLElement* colors = visual->FirstChildElement("Colors");
                if (colors) {
                    for (tinyxml2::XMLElement* colorElem = colors->FirstChildElement("Color"); colorElem; colorElem = colorElem->NextSiblingElement("Color")) {
                        float r = std::stof(colorElem->Attribute("R")) / 255.0f;
                        float g = std::stof(colorElem->Attribute("G")) / 255.0f;
                        float b = std::stof(colorElem->Attribute("B")) / 255.0f;
                        PFAConfig.push_back(ColorData{ r, g, b, 1.0f });
                    }
                }
            }
        }
    }
    else {
        std::cerr << "Failed to load PFA Config from " << path << std::endl;
    }

    std::cout << "Loaded " << PFAConfig.size() << " colors." << std::endl;

    trackColors.resize(tracks);
    srand(static_cast<unsigned int>(time(nullptr)));

    bool colorLoop = false; // replace with actual configuration
    if (!colorLoop || PFAConfig.empty()) {
        for (int i = 0; i < tracks; ++i) {
            if (i < PFAConfig.size()) {
                trackColors[i] = PFAConfig[i];
            }
            else {
                float h = static_cast<float>(rand()) / RAND_MAX;
                float s = static_cast<float>(rand()) / RAND_MAX * 0.4f + 0.6f;
                float v = static_cast<float>(rand()) / RAND_MAX * 0.2f + 0.8f;
                trackColors[i] = HSVToRGB(h, s, v);
            }
        }
    }
    else {
        for (int i = 0; i < tracks; ++i) {
            trackColors[i] = PFAConfig[i % PFAConfig.size()];
        }
    }

    Ready = true;
}
