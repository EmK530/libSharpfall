#include "ConMIDI.h"
#include "Synth.h"
#include "Clock.h"
#include "BufferFile.h"
#include "EventThread.h"
#include "ObjectManager.h"
#include <stdio.h>
#include <memory>

std::vector<std::unique_ptr<unsigned char[]>> trackData;
std::vector<int> trackSizes;
std::vector<unsigned char*> trackReader;
std::vector<unsigned long long> trackPosition;
std::vector<byte> prevEvent;
std::vector<bool> trackFinished;

int trackCount = 0;
int realTracks = 0;
int ppq = 0;

int lastPos = 0;
int lastSize = 0;

bool loaded = false;

Clock midiClock;

int CopyTrack(BufferFile* buf, int id)
{
    if (lastPos != 0)
    {
        buf->seek(lastPos + lastSize + 8);
    }
    lastPos = buf->getFilePos() + buf->getBufPos();
    if (!buf->textSearch("MTrk"))
    {
        printf("Could not find track %i\n", id);
        return 0;
    }
    printf("Copying track %i / %i\n", realTracks + 1, trackCount);
    realTracks++;

    lastSize = 0;
    for (int i = 0; i < 4; ++i)
        lastSize = (lastSize << 8) | buf->readByte();

    int sz = lastSize;
    int offset = 0;

    trackData.push_back(std::make_unique<unsigned char[]>(sz));
    trackSizes.push_back(sz);
    auto& trackBuf = trackData.back();

    while (sz > 0)
    {
        unsigned long use = std::min<unsigned long>(sz, buf->getBufRange() - buf->getBufPos());
        buf->copy(trackBuf.get(), offset, use);
        offset += use;
        sz -= use;
    }
    return 1;
}

void handleSysEx(unsigned char*& tR) {
    size_t size = 64;
    unsigned char* arr = (unsigned char*)malloc(size);
    if (!arr) return;

    size_t pos = 1;
    arr[0] = 0xF0;
    tR++;

    while ((arr[pos] = *(tR++)) != 0xF7) {
        pos++;
        if (pos >= size) {
            size *= 2;
            unsigned char* tmp = (unsigned char*)realloc(arr, size);
            if (!tmp) { free(arr); return; }
            arr = tmp;
        }
    }

    MIDIHDR longdata{};
    longdata.lpData = (LPSTR)arr;
    longdata.dwBufferLength = static_cast<DWORD>(pos + 1);
    longdata.dwBytesRecorded = static_cast<DWORD>(pos + 1);
    longdata.dwFlags = 0;

    UINT error = PrepareLongData(&longdata, sizeof(longdata));
    if (!error) {
        error = SendDirectLongData(&longdata, sizeof(longdata));
        if (error) {
            
        }

        while (MIDIERR_STILLPLAYING == UnprepareLongData(&longdata, sizeof(longdata))) {
            
        }
    }

    free(arr);
}

extern "C"
{
    __declspec(dllexport) int CM_InitSynth()
    {
        return Sound_Init();
    }

    __declspec(dllexport) int CM_ReloadSynth()
    {
        return Sound_Reload();
    }

    __declspec(dllexport) void CM_Dispose()
    {
        EventThread_Shutdown();

        loaded = false;
        lastPos = 0;
        lastSize = 0;

        trackData.clear();
        trackReader.clear();
        trackFinished.clear();
        trackPosition.clear();
        prevEvent.clear();

        realTracks = 0;
        trackCount = 0;
        ppq = 0;

        running.store(false);
    }

    __declspec(dllexport) int CM_LoadMIDIPath(const char* path)
    {
        if (loaded)
            CM_Dispose();
        BufferFile buf(path, 8192);
        if (!buf.textSearch("MThd"))
        {
            MessageBoxA(0, "Failed to load MIDI, header not found.", "libSharpfall Warning", 0x00000030);
            printf("Not a MIDI file.\n");
            return 0;
        }
        buf.skip(6);
        trackCount = buf.readByte() * 256 + buf.readByte();
        ppq = buf.readByte() * 256 + buf.readByte();

        printf("Tracks: %i\n", trackCount);
        printf("PPQ: %i\n", ppq);

        buf.resizeBuffer(16000000); // 16 MB

        int i = 0;
        for (i = 0; i < trackCount; i++)
        {
            if (!CopyTrack(&buf, i))
                break;
        }

        realTracks = i;

        if (realTracks == 0)
        {
            MessageBoxA(0, "Failed to load MIDI, no tracks were copied.", "libSharpfall Warning", 0x00000030);
            return 0;
        }

        trackReader.resize(realTracks);
        trackPosition.resize(realTracks, 0);
        prevEvent.resize(realTracks, 0);
        trackFinished.resize(realTracks, false);

        for (int t = 0; t < realTracks; t++)
            trackReader[t] = trackData[t].get();

        midiClock = Clock(ppq, 120);
        loaded = true;

        return realTracks > 0 ? 1 : 0;
    }

    __declspec(dllexport) int CM_StepPlayer(double deltaTime)
    {
        if (!loaded)
            return 2;

        if (!running.load()) {
            EventThread_Init();
        }

        SafetyCheck();
        NewFrame();

        midiClock.Step(deltaTime);
        unsigned long long clockUInt64 = static_cast<unsigned long long>(midiClock.GetTick());

        unsigned int aliveTracks = realTracks;

        bool ran = false;
        for (unsigned int i = 0; i < realTracks; i++)
        {
            if (trackFinished[i]) continue;
            ran = true;

            unsigned char*& tR = trackReader[i];
            unsigned char* trackBounds = trackData[i].get() + trackSizes[i];
            unsigned long long tempPos = trackPosition[i];
            byte tempPrev = prevEvent[i];

            bool doloop = true;
            while (doloop)
            {
                if (tR == trackBounds) {
                    //printf("[WARN] Track %u abruptly ended with no End of Track event!\n", i + 1);
                    trackFinished[i] = TRUE;
                    aliveTracks--;
                    break;
                }
                unsigned char* startPos = tR;
                unsigned long val = 0;
                unsigned char temp = 0;

                do {
                    temp = *(tR++);
                    val = (val << 7) | (temp & 0x7F);
                } while (temp & 0x80);

                tempPos += val;
                if (tempPos > clockUInt64) {
                    tR = startPos;
                    tempPos -= val;
                    break;
                }

                byte readEvent = *(tR++);
                if (readEvent < 0x80)
                {
                    tR--;
                    readEvent = tempPrev;
                }
                tempPrev = readEvent;
                int trackEvent = readEvent & 0b11110000;
                if (trackEvent == 0x90)
                {
                    byte note = *(tR++);
                    byte vel = *(tR++);
                    if (vel > 0)
                        SubmitNote(clockUInt64, i, note, vel);
                    EventThread_QueueEvent(readEvent | (note << 8) | (vel << 16));
                }
                else if (trackEvent == 0x80)
                {
                    byte note = *(tR++);
                    byte vel = *(tR++);
                    EventThread_QueueEvent(readEvent | (note << 8) | (vel << 16));
                }
                else if (trackEvent == 0xA0 || trackEvent == 0xE0 || trackEvent == 0xB0)
                {
                    byte note = *(tR++);
                    byte vel = *(tR++);
                    SendDirectData(readEvent | (note << 8) | (vel << 16));
                }
                else if (trackEvent == 0xC0 || trackEvent == 0xD0)
                {
                    SendDirectData((readEvent | (*(tR++) << 8)));
                }
                else if (trackEvent == 0)
                {
                    break;
                }
                else {
                    switch (readEvent)
                    {
                    case 0b11110000: {
                        handleSysEx(tR);
                        break;
                    }
                    case 0b11110010:
                        tR += 2;
                        break;
                    case 0b11110011:
                        tR++;
                        break;
                    case 0xFF: {
                        readEvent = *(tR++);
                        switch (readEvent) {
                        case 0x51:
                        {
                            tR++;
                            unsigned long int event = 0;
                            for (int i = 0; i != 3; i++) {
                                byte temp = *(tR++);
                                event = (event << 8) | temp;
                            }
                            midiClock.SubmitBPM(tempPos, event);
                            break;
                        }
                        case 0x2F:
                        {
                            doloop = FALSE;
                            trackFinished[i] = true;
                            aliveTracks--;
                            break;
                        }
                        default:
                        {
                            tR += *(tR++);
                            break;
                        }
                        }
                    }
                    }
                }
            }
            trackPosition[i] = tempPos;
            prevEvent[i] = tempPrev;
            trackReader[i] = tR;
        }

        if (!ran)
        {
            return 0;
        }
        return 1;
    }
}