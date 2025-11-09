#include "ConMIDI.h"
#include "PhysXUnity.h"
#include <stdio.h>
#include <Windows.h>
#include <WinUser.h>

#define libSharpfall_TARGET "Sharpfall v2.0.2-nativelib";
#define libSharpfall_VER "BETA 1";
#define PhysXUnity_VER "BETA 2";
#define ConMIDI_VER "v3.0.0 (v3-b401 C++ Port)";

static ConMIDI _ConMIDI;
static PhysXUnity _PhysXUnity;

#ifdef _DEBUG

#include <iostream>
#include <windows.h>

int main()
{
    std::cout << "libSharpfall DEBUG\n\n";

    //_PhysXUnity.InitPhysics();
    //printf("\n");
    _ConMIDI.InitSynth();
    printf("\n");
    _ConMIDI.LoadMIDIPath("E:/MIDIs/Twinkle, Twinkle, Little Star.mid");

    while (_ConMIDI.StepPlayer(1.0f / 550.0f))
    {
        Sleep(1);
    }
}

#else

extern "C" {
    // libSharpfall
    __declspec(dllexport) const char* GetTarget_libSharpfall()
    {
        return libSharpfall_TARGET;
    }
    __declspec(dllexport) const char* GetVer_libSharpfall()
    {
        return libSharpfall_VER;
    }
    __declspec(dllexport) const char* GetVer_PhysXUnity()
    {
        return PhysXUnity_VER;
    }
    __declspec(dllexport) const char* GetVer_ConMIDI()
    {
        return ConMIDI_VER;
    }

    // ConMIDI
    __declspec(dllexport) int InitSynth()
    {
        return _ConMIDI.InitSynth();
    }
    __declspec(dllexport) int ReloadSynth()
    {
        return _ConMIDI.ReloadSynth();
    }
    __declspec(dllexport) int LoadMIDIPath(const char* path)
    {
        return _ConMIDI.LoadMIDIPath(path);
    }
    __declspec(dllexport) int StepPlayer(double deltaTime)
    {
        return _ConMIDI.StepPlayer(deltaTime);
    }
    __declspec(dllexport) void Dispose()
    {
        return _ConMIDI.Dispose();
    }

    // PhysXUnity
    __declspec(dllexport) bool GetCUDAStatus()
    {
        return _PhysXUnity.GetCUDAStatus();
    }
    __declspec(dllexport) const char* GetCUDADevice()
    {
        return _PhysXUnity.GetCUDADevice();
    }
    __declspec(dllexport) void InitPhysics()
    {
        MessageBoxA(0, "OKAY WAIT HERE WE GO", "libSharpfall Warning", 0x00000030);
        _PhysXUnity.InitPhysics();
        MessageBoxA(0, "WE MADE IT", "libSharpfall Warning", 0x00000030);
    }
    __declspec(dllexport) void DeleteAllObjects()
    {
        _PhysXUnity.DeleteAllObjects();
    }
    __declspec(dllexport) void StepPhysics(float deltaTime)
    {
        _PhysXUnity.StepPhysics(deltaTime);
    }
    __declspec(dllexport) int GetRendererData(ObjectData* buffer, int bufferSize)
    {
        return _PhysXUnity.GetAllObjectTransforms(buffer, bufferSize);
    }
    __declspec(dllexport) int GetObjectCount()
    {
        return _PhysXUnity.GetObjectCount();
    }
    __declspec(dllexport) void ShutdownPhysics()
    {
        _PhysXUnity.ShutdownPhysics();
    }
}

#endif