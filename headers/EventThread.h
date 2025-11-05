#pragma once
#include <thread>
#include <atomic>
#include <cstdint>
#include "UniSleep.h"
#include "Synth.h"

constexpr size_t BUFFER_SIZE = 65536;

unsigned long eventBuffer[BUFFER_SIZE];
unsigned long* const bufferStart = &eventBuffer[0];
unsigned long* const bufferEnd = &eventBuffer[BUFFER_SIZE];

std::atomic<bool> running{ false };
std::thread eventThread;
std::atomic<unsigned long*> writePtr{ bufferStart };

// Initialize thread
inline void EventThread_Init()
{
    writePtr.store(bufferStart, std::memory_order_relaxed);
    running.store(true, std::memory_order_relaxed);

    eventThread = std::thread([]() {
        unsigned long* readPtr = bufferStart;

        while (running.load(std::memory_order_relaxed))
        {
            while (readPtr != writePtr.load(std::memory_order_acquire))
            {
                SendDirectData(*readPtr);
                ++readPtr;
                if (readPtr == bufferEnd) readPtr = bufferStart;
            }
            uniSleep(1);
        }
        });
}

inline void EventThread_QueueEvent(unsigned long data)
{
    *writePtr.load(std::memory_order_relaxed) = data;
    unsigned long* next = writePtr.load(std::memory_order_relaxed) + 1;
    if (next == bufferEnd) next = bufferStart;
    writePtr.store(next, std::memory_order_release);
}

inline void EventThread_Shutdown()
{
    running.store(false, std::memory_order_relaxed);
    if (eventThread.joinable())
        eventThread.join();
}