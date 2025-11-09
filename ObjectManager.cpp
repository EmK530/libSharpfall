#include <Windows.h>
#include "ObjectManager.h"
#include "PhysXUnity.h"
#include <deque>

static PhysXUnity _PhysXUnity;

struct ColorData { float r, g, b, a; };
static std::vector<ColorData> gColor;

enum NoDoublesMode
{
    None = 0,
    PerFrameDynamic = 1,
    Smart2 = 2,
    Smart5 = 3
};

NoDoublesMode limiter = PerFrameDynamic;

size_t blockLimit = 10000;
size_t spawnsThisFrame = 0;

// PerFrameDynamic
size_t perNoteLimit = 1;
uint64_t totalSpawns = 0;

// Smart
static std::vector<int> thresholds;
static std::deque<int> frameHistory;
static int overloadLevel = 0;
static const int maxHistoryFrames = 20;

std::unordered_map<char, size_t> noteCounts;

extern "C"
{
    __declspec(dllexport) int OM_GetAllColorData(ColorData* buffer, int bufferSize)
    {
        int count = min(bufferSize, (int)gColor.size());
        for (int i = 0; i < count; i++)
        {
            ColorData c = gColor[i];
            buffer[i].r = c.r; buffer[i].g = c.g; buffer[i].b = c.b; buffer[i].a = c.a;
        }
        return 1;
    }
}

void NewFrame()
{
    if (limiter == Smart2 || limiter == Smart5)
    {
        frameHistory.push_back((int)spawnsThisFrame);
        if (frameHistory.size() > maxHistoryFrames)
            frameHistory.pop_front();

        int totalRecent = 0;
        for (int c : frameHistory)
            totalRecent += c;

        overloadLevel = 0;
        for (int t : thresholds)
            if (totalRecent > t)
                overloadLevel++;
    }

    noteCounts.clear();
    spawnsThisFrame = 0;
}

void ResetOM()
{
    frameHistory.clear();
    switch (limiter)
    {
        case Smart2:
            thresholds = { 660 };
            break;
        case Smart5:
            thresholds = { 128, 256, 384, 512 };
            break;
        default:
            thresholds = {};
    }
    gColor.clear();
    totalSpawns = 0;
    NewFrame();
}

void SafetyCheck()
{
    if (_PhysXUnity.GetObjectCount() != gColor.size())
    {
        ResetOM();
    }
}

ColorData HSVtoRGB(float h, float s, float v, float alpha = 1.0f)
{
    ColorData color;
    color.a = alpha;

    if (s <= 0.0f) {
        color.r = color.g = color.b = v;
        return color;
    }

    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float hf = h / 60.0f;
    int i = static_cast<int>(hf);
    float f = hf - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i) {
    case 0: color.r = v; color.g = t; color.b = p; break;
    case 1: color.r = q; color.g = v; color.b = p; break;
    case 2: color.r = p; color.g = v; color.b = t; break;
    case 3: color.r = p; color.g = q; color.b = v; break;
    case 4: color.r = t; color.g = p; color.b = v; break;
    case 5: default: color.r = v; color.g = p; color.b = q; break;
    }

    return color;
}

void SubmitNote(unsigned long long clock, int track, char note, char vel)
{
    switch (limiter)
    {
        case PerFrameDynamic:
        {
            size_t count = noteCounts[note];
            if (count >= perNoteLimit)
                return;
            noteCounts[note]++;
            break;
        }
        // surely this isn't correct, but it's something
        case Smart2:
        case Smart5:
        {
            float scale = 1.0f - (overloadLevel * 0.15f);
            if (scale < 0.25f) scale = 0.25f;
            size_t dynamicLimit = (size_t)(blockLimit * scale);

            if (spawnsThisFrame >= dynamicLimit)
                return;
            break;
        }

    }

    int index = (int)(totalSpawns % blockLimit);
    if (totalSpawns < blockLimit)
    {
        // New block
        _PhysXUnity.CreateObject(
            note * 0.1f + (-6.0f),
            7.0f,
            0.0f,
            0.0f, // todo: vel.x
            -15.0f, // todo: vel.y
            0.0f // todo: vel.z
        );

        gColor.push_back(HSVtoRGB(note / 0.35555555555f, 1.0f, 1.0f));
    }
    else
    {
        // Reuse existing block (circular)
        _PhysXUnity.SetObjectTransform(
            index,
            note * 0.1f + (-6.0f),
            7.0f,
            0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, // todo: vel.x
            -15.0f, // todo: vel.y
            0.0f // todo: vel.z
        );
        gColor[index] = HSVtoRGB(note / 0.35555555555f, 1.0f, 1.0f);
    }

    totalSpawns++;
    spawnsThisFrame++;
}
