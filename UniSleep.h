#pragma once

#ifdef _WIN32
#include <windows.h>
void uniSleep(unsigned long int duration)
{
    Sleep(duration);
}
#elif __linux__
#include <unistd.h>
void uniSleep(unsigned long int duration)
{
    sleep(duration * 1000);
}
#else
void uniSleep(unsigned long int duration) {}
#endif