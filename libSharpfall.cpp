#include "PhysXUnity.h"
#include "ConMIDI.h"

#define libSharpfall_TARGET "^Sharpfall v3-indev";
#define libSharpfall_VER "INDEV";
#define PhysXUnity_VER "INDEV";
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
    __declspec(dllexport) const char* LS_GetVer_PhysXUnity()
    {
        return PhysXUnity_VER;
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