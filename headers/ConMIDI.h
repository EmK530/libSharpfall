#pragma once

class ConMIDI {
public:
	int CM_InitSynth();
	int CM_ReloadSynth();
	int CM_LoadMIDIPath(const char* path);
	int CM_StepPlayer(double deltaTime);
	void CM_Dispose();
};