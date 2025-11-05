#include "PhysXUnity.h"
#include "ConMIDI.h"

#define libSharpfall_TARGET "v3*";
#define libSharpfall_VER "BETA 1";
#define PhysXUnity_VER "INDEV";
#define ConMIDI_VER "v3.0.0 (v3-b401 C++ Port)";

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
}