#pragma once

struct ObjectData { float x, y, z; float qx, qy, qz, qw; };

class PhysXUnity {
public:
    void InitPhysics();
    int CreateObject(float x, float y, float z, float vx, float vy, float vz);
    void DeleteAllObjects();
    void StepPhysics(float deltaTime);
    int GetAllObjectTransforms(ObjectData* buffer, int bufferSize);
    void SetObjectTransform(int index, float x, float y, float z, float qx, float qy, float qz, float qw, float vx, float vy, float vz);
    int GetObjectCount();
    void ShutdownPhysics();

    bool GetCUDAStatus();
    const char* GetCUDADevice();
};