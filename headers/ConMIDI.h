#pragma once

class ConMIDI {
public:
	int InitSynth();
	int ReloadSynth();
	int LoadMIDIPath(const char* path);
	int StepPlayer(double deltaTime);
	void Dispose();
};