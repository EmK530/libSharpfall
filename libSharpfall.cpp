#define _CRT_SECURE_NO_WARNINGS
#include "headers\PhysXUnity.h"

#define libSharpfall_TARGET "LV7";
#define libSharpfall_VER "v1.0.0-main-2026042101";
#define ConMIDI_VER "v3.0.1";

#define VALIDATION "cd629ef8b2064d09"

#include <windows.h>
#include <stdio.h>

void AttachConsoleForLogging()
{
    if (AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        SetConsoleTitleA("libSharpfall Debug Console");
    }
}

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
		//AttachConsoleForLogging();
        return VALIDATION;
    }
}