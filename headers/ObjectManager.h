#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>

void NewFrame();
void ResetOM();
void SubmitNote(unsigned long long clock, int track, char note, char vel);
void SafetyCheck();