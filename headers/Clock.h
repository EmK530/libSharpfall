#pragma once
#include <ctime>

class Clock
{
public:
    Clock(unsigned int ppq = 480, double bpmInit = 120.0)
        : cppq(ppq), bpm(bpmInit)
    {
        ticklen = (1.0 / cppq) * (60.0 / bpm);
        timee = 0.0;
        throttle = true;
        timeLost = 0.0;
        lastElapsed = 0.0;
    }

    void Step(double deltaTime)
    {
        double temp = deltaTime;

        if (throttle)
        {
            if (temp - lastElapsed > 0.0333333)
            {
                timeLost += (temp - lastElapsed) - 0.0333333;
                lastElapsed = temp;
                temp = temp - timeLost;
            }
        }

        lastElapsed = deltaTime;
        timee += temp / ticklen;
    }

    void SubmitBPM(double tickPos, unsigned long int microsecondsPerQuarter)
    {
        double remainder = timee - tickPos;
        timee = tickPos;
        bpm = 60000000.0 / microsecondsPerQuarter;
        ticklen = (1.0 / cppq) * (60.0 / bpm);
        timee += remainder;
    }

    double GetTick() const
    {
        return timee;
    }

    void Reset()
    {
        timee = 0.0;
        lastElapsed = 0.0;
        timeLost = 0.0;
    }

    double timee;
    unsigned int cppq;
    double bpm;
    double ticklen;
    bool throttle;
    double timeLost;
    double lastElapsed;
};