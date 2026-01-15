#pragma once

struct UnityMatrix
{
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
};

class PhysXUnity {
public:
    int InitPhysics();
    int CreateObject(float x, float y, float z, float vx, float vy, float vz);
    void DeleteAllObjects();
    void WriteNewObjectLimit();
    void BeginStep(float deltaTime);
    void IsStepDone();
    void CompleteStep();
    void StepPhysics(float deltaTime);
    int GetAllObjectMatrices(UnityMatrix* buffer, int bufferSize);
    void SetObjectTransform(int index, float x, float y, float z, float qx, float qy, float qz, float qw, float vx, float vy, float vz);
    int GetObjectCount();
    void ShutdownPhysics();

    bool GetCUDAStatus();
    const char* GetCUDADevice();
};