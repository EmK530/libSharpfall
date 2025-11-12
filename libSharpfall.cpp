#include "headers\PhysXUnity.h"
#include "headers\ConMIDI.h"

#define libSharpfall_TARGET "LV2";
#define libSharpfall_VER "indev-2025111200";
#define ConMIDI_VER "v3.0.0 (v3-b401 C++ Port)";

#define VALIDATION "cd629ef8b2064d09"

extern "C" {
    // ConMIDI and PhysXUnity functions are defined in their respective scripts
    __declspec(dllexport) const char* LS_GetTarget_libSharpfall()
    {
        return libSharpfall_TARGET;
    }
    __declspec(dllexport) const char* LS_GetVer_libSharpfall()
    {
        return libSharpfall_VER;
    }
    __declspec(dllexport) const char* LS_GetVer_ConMIDI()
    {
        return ConMIDI_VER;
    }

    __declspec(dllexport) const char* LS_GetValidation()
    {
        return VALIDATION;
    }
}