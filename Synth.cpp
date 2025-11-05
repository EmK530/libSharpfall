#include "KDMAPI.h"
#include <stdio.h>
#include <WinUser.h>

void (*SendDirectData)(unsigned long int);
int (*SendDirectLongData)(MIDIHDR* a, unsigned int b);
int (*PrepareLongData)(MIDIHDR* a, unsigned int b);
int (*UnprepareLongData)(MIDIHDR* a, unsigned int b);

bool KDMAPI_loaded = false;

int Sound_Init() {
    if (KDMAPI_Setup() == 0) {
        MessageBoxA(0, "Could not initialize OmniMIDI, you might run into issues.", "libSharpfall Error", 0x00000010);
        return 0;
    };

    KDMAPI_InitializeKDMAPIStream();
    KDMAPI_loaded = true;
    SendDirectData = KDMAPI_SendDirectData;
    SendDirectLongData = KDMAPI_SendDirectLongData;
    PrepareLongData = KDMAPI_PrepareLongData;
    UnprepareLongData = KDMAPI_UnprepareLongData;
    return 1;
}

int Sound_Reload() {
    if (!KDMAPI_loaded)
        return 0;
    KDMAPI_ResetKDMAPIStream();
    return 1;
}