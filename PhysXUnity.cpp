#include <vector>
#include <PxPhysicsAPI.h>
#include <gpu/PxPhysicsGpu.h>
#include <cuda_runtime.h>
#include <Windows.h>

#include "PhysXUnity.h"
#include "ObjectManager.h"

using namespace physx;

// PhysX globals
static PxFoundation* mFoundation = nullptr;
static PxPhysics* mPhysics = nullptr;
static PxScene* mScene = nullptr;
static PxMaterial* mMaterial = nullptr;
static physx::PxCudaContextManager* mCudaContextManager;

static std::vector<PxRigidDynamic*> gActors;

bool CUDA = false;
const char* CUDA_device = "N/A";

int solverIterations = 32;
int subStepTargetFPS = 60;

extern "C"
{
    __declspec(dllexport) bool PXU_GetCUDAStatus() { return CUDA; }
    __declspec(dllexport) const char* PXU_GetCUDADevice() { return CUDA_device; }

    __declspec(dllexport) void PXU_InitPhysics()
    {
        if (mScene)
            return;

        static PxDefaultAllocator gAllocator;
        static PxDefaultErrorCallback gErrorCallback;

        // Create foundation
        mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
        if (!mFoundation)
        {
            return;
        }

        // Create physics
        mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale());
        if (!mPhysics)
        {
            return;
        }

        // Create default material
        mMaterial = mPhysics->createMaterial(0.25f, 0.25f, 0.15f);

        // Scene descriptor
        PxSceneDesc sceneDesc(mPhysics->getTolerancesScale());
        sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
        sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(16);
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        // Check for CUDA devices
        int deviceCount = 0;
        cudaGetDeviceCount(&deviceCount);
        if (deviceCount == 0)
        {
            MessageBoxA(0, "No CUDA devices detected.", "libSharpfall Warning", MB_ICONWARNING);
        }
        else
        {
            cudaDeviceProp deviceProp;
            cudaGetDeviceProperties(&deviceProp, 0);
            CUDA_device = deviceProp.name;
            //MessageBoxA(0, deviceProp.name, "CUDA Device Found", MB_ICONINFORMATION);

            // Create CUDA context
            PxCudaContextManagerDesc cudaContextManagerDesc;
            mCudaContextManager = PxCreateCudaContextManager(*mFoundation, cudaContextManagerDesc, nullptr);
            if (mCudaContextManager && mCudaContextManager->contextIsValid())
            {
                CUDA = true;
                //MessageBoxA(0, "GPU acceleration enabled.", "libSharpfall Warning", MB_ICONINFORMATION);

                if (mCudaContextManager)
                {
                    sceneDesc.cudaContextManager = mCudaContextManager;
                    sceneDesc.flags |= PxSceneFlag::eENABLE_GPU_DYNAMICS;
                    sceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
                    sceneDesc.flags |= PxSceneFlag::eDISABLE_CCD_RESWEEP;
                    sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU;

                    PxGpuDynamicsMemoryConfig gpuMemoryConfig;
                    gpuMemoryConfig.maxRigidContactCount = 1024 * 512 * 8;
                    gpuMemoryConfig.maxRigidPatchCount = 1024 * 80 * 8;
                    gpuMemoryConfig.foundLostPairsCapacity = 256 * 1024 * 8;
                    sceneDesc.gpuDynamicsConfig = gpuMemoryConfig;
                }
            }
            else
            {
                if (mCudaContextManager)
                {
                    mCudaContextManager->release();
                    mCudaContextManager = nullptr;
                }
                //MessageBoxA(0, "GPU acceleration disabled.", "libSharpfall Warning", MB_ICONWARNING);
            }
        }

        // Create scene
        mScene = mPhysics->createScene(sceneDesc);
        if (!mScene)
        {
            MessageBoxA(0, "PxCreateScene failed!", "libSharpfall Warning", MB_ICONERROR);
            return;
        }

        gActors.clear();

        // Create static platform
        PxTransform platformTransform(PxVec3(-0.5f, -5.0f, 0.0f));
        PxBoxGeometry platformGeom(10.0f, 5.0f, 10.0f);
        PxMaterial* platformMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.10f);
        PxRigidStatic* platform = PxCreateStatic(*mPhysics, platformTransform, platformGeom, *platformMaterial);
        mScene->addActor(*platform);
    }

    __declspec(dllexport) int PXU_CreateObject(float x, float y, float z, float vx, float vy, float vz)
    {
        PxRigidDynamic* body = mPhysics->createRigidDynamic(PxTransform(PxVec3(x, y, z)));
        PxShape* shape = mPhysics->createShape(PxBoxGeometry(0.05f, 0.5f, 0.5f), *mMaterial);

        body->attachShape(*shape);

        body->setSolverIterationCounts(solverIterations, 1);

        body->setLinearVelocity(PxVec3(vx, vy, vz));
        PxRigidBodyExt::updateMassAndInertia(*body, 1.0f);
        mScene->addActor(*body);
        gActors.push_back(body);
        return static_cast<int>(gActors.size() - 1);
    }

    __declspec(dllexport) void PXU_DeleteAllObjects()
    {
        for (auto a : gActors) { mScene->removeActor(*a); a->release(); }
        gActors.clear();
    }

    __declspec(dllexport) void PXU_StepPhysics(float deltaTime)
    {
        float subStepTarget = 1.0f / (float)subStepTargetFPS;
        int subSteps = max((deltaTime / subStepTarget), 1);
        for (int i = 0; i < subSteps; i++)
        {
            mScene->simulate(deltaTime / (float)subSteps);
            mScene->fetchResults(true);
        }
    }

    __declspec(dllexport) int PXU_GetAllObjectTransforms(ObjectData* buffer, int bufferSize)
    {
        int count = min(bufferSize, (int)gActors.size());
        for (int i = 0;i < count;i++)
        {
            PxTransform t = gActors[i]->getGlobalPose();
            buffer[i].x = t.p.x; buffer[i].y = t.p.y; buffer[i].z = t.p.z;
            buffer[i].qx = t.q.x; buffer[i].qy = t.q.y; buffer[i].qz = t.q.z; buffer[i].qw = t.q.w;
        }
        return 1;
    }

    __declspec(dllexport) void PXU_SetObjectTransform(int index, float x, float y, float z, float qx, float qy, float qz, float qw, float vx, float vy, float vz)
    {
        if (index < 0 || index >= (int)gActors.size())
            return;

        PxRigidDynamic* actor = gActors[index];
        if (!actor)
            return;

        PxQuat quat(qx, qy, qz, qw);
        PxTransform t(PxVec3(x, y, z), quat);
        actor->setGlobalPose(t);

        actor->setLinearVelocity(PxVec3(vx, vy, vz));
        actor->setAngularVelocity(PxVec3(0.0f));
    }

    __declspec(dllexport) int PXU_GetObjectCount() { return (int)gActors.size(); }

    __declspec(dllexport) void PXU_ShutdownPhysics()
    {
        // not solved yet, releasing causes Unity Editor crash
        PXU_DeleteAllObjects();
    }
}

void PhysXUnity::InitPhysics()
{
    PXU_InitPhysics();
}
int PhysXUnity::CreateObject(float x, float y, float z, float vx, float vy, float vz)
{
    return PXU_CreateObject(x, y, z, vx, vy, vz);
}
void PhysXUnity::DeleteAllObjects()
{
    PXU_DeleteAllObjects();
}
void PhysXUnity::StepPhysics(float deltaTime)
{
    PXU_StepPhysics(deltaTime);
}
int PhysXUnity::GetAllObjectTransforms(ObjectData* buffer, int bufferSize)
{
    return PXU_GetAllObjectTransforms(buffer, bufferSize);
}
void PhysXUnity::SetObjectTransform(int index, float x, float y, float z, float qx, float qy, float qz, float qw, float vx, float vy, float vz)
{
    PXU_SetObjectTransform(index, x, y, z, qx, qy, qz, qw, vx, vy, vz);
}
int PhysXUnity::GetObjectCount()
{
    return PXU_GetObjectCount();
}
void PhysXUnity::ShutdownPhysics()
{
    PXU_ShutdownPhysics();
}
bool PhysXUnity::GetCUDAStatus()
{
    return PXU_GetCUDAStatus();
}
const char* PhysXUnity::GetCUDADevice()
{
    return PXU_GetCUDADevice();
}