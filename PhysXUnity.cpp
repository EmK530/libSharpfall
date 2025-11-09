#include <vector>
#include <PxPhysicsAPI.h>
#include <gpu/PxPhysicsGpu.h>
#include <cuda_runtime.h>
#include <Windows.h>

#include "PhysXUnity.h"

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

bool PhysXUnity::GetCUDAStatus() { return CUDA; }
const char* PhysXUnity::GetCUDADevice() { return CUDA_device; }

void PhysXUnity::InitPhysics()
{
    if (mScene)
        return;

    // Local static allocator & error callback to avoid Unity crashes
    static PxDefaultAllocator gAllocator;
    static PxDefaultErrorCallback gErrorCallback;

    // Create foundation
    mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
    if (!mFoundation)
    {
        MessageBoxA(0, "PxCreateFoundation failed!", "libSharpfall Warning", MB_ICONERROR);
        return;
    }
    MessageBoxA(0, "PxFoundation created", "libSharpfall Warning", MB_ICONINFORMATION);

    // Create physics
    mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale());
    if (!mPhysics)
    {
        MessageBoxA(0, "PxCreatePhysics failed!", "libSharpfall Warning", MB_ICONERROR);
        return;
    }
    MessageBoxA(0, "PxPhysics created", "libSharpfall Warning", MB_ICONINFORMATION);

    // Create default material
    mMaterial = mPhysics->createMaterial(0.25f, 0.25f, 0.10f);
    MessageBoxA(0, "PxMaterial created", "libSharpfall Warning", MB_ICONINFORMATION);

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
        MessageBoxA(0, deviceProp.name, "CUDA Device Found", MB_ICONINFORMATION);
    }

    // Create CUDA context
    PxCudaContextManagerDesc cudaContextManagerDesc;
    mCudaContextManager = PxCreateCudaContextManager(*mFoundation, cudaContextManagerDesc, nullptr);
    if (mCudaContextManager && mCudaContextManager->contextIsValid())
    {
        CUDA = true;
        MessageBoxA(0, "GPU acceleration enabled.", "libSharpfall Warning", MB_ICONINFORMATION);
    }
    else
    {
        if (mCudaContextManager)
        {
            mCudaContextManager->release();
            mCudaContextManager = nullptr;
        }
        MessageBoxA(0, "GPU acceleration disabled.", "libSharpfall Warning", MB_ICONWARNING);
    }

    // Scene descriptor
    PxSceneDesc sceneDesc(mPhysics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(16);
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;

    if (mCudaContextManager)
    {
        sceneDesc.cudaContextManager = mCudaContextManager;
        sceneDesc.flags |= PxSceneFlag::eENABLE_GPU_DYNAMICS;
        sceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
        sceneDesc.flags |= PxSceneFlag::eDISABLE_CCD_RESWEEP;
        sceneDesc.flags &= ~PxSceneFlag::eENABLE_CCD;
        sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU;

        PxGpuDynamicsMemoryConfig gpuMemoryConfig;
        gpuMemoryConfig.maxRigidContactCount = 1024 * 512 * 16;
        gpuMemoryConfig.maxRigidPatchCount = 1024 * 64 * 16;
        gpuMemoryConfig.foundLostPairsCapacity = 8 * 1024 * 1024;
        sceneDesc.gpuDynamicsConfig = gpuMemoryConfig;
    }

    // Create scene
    mScene = mPhysics->createScene(sceneDesc);
    if (!mScene)
    {
        MessageBoxA(0, "PxCreateScene failed!", "libSharpfall Warning", MB_ICONERROR);
        return;
    }
    MessageBoxA(0, "PhysX scene created", "libSharpfall Warning", MB_ICONINFORMATION);

    gActors.clear();

    // Create static platform
    PxTransform platformTransform(PxVec3(-0.5f, -5.0f, 0.0f));
    PxBoxGeometry platformGeom(10.0f, 5.0f, 10.0f);
    PxMaterial* platformMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.10f);
    PxRigidStatic* platform = PxCreateStatic(*mPhysics, platformTransform, platformGeom, *platformMaterial);
    mScene->addActor(*platform);

    MessageBoxA(0, "Initialized physics scene.", "libSharpfall Warning", MB_ICONINFORMATION);
}

int PhysXUnity::CreateObject(float x, float y, float z, float vx, float vy, float vz)
{
    PxRigidDynamic* body = mPhysics->createRigidDynamic(PxTransform(PxVec3(x, y, z)));
    PxShape* shape = mPhysics->createShape(PxBoxGeometry(0.05f, 0.5f, 0.5f), *mMaterial);

    body->attachShape(*shape);

    /*
    this is a lot but the best fine tune I was able to get
    too little will have blocks sink into eachother and retain lag
    at 32 the blocks are reactive enough to push eachother apart
    */
    body->setSolverIterationCounts(32, 1);

    body->setLinearVelocity(PxVec3(vx, vy, vz));
    PxRigidBodyExt::updateMassAndInertia(*body, 1.0f);
    mScene->addActor(*body);
    gActors.push_back(body);
    return static_cast<int>(gActors.size() - 1);
}

void PhysXUnity::DeleteAllObjects()
{
    for (auto a : gActors) { mScene->removeActor(*a); a->release(); }
    gActors.clear();
}

void PhysXUnity::StepPhysics(float deltaTime)
{
    mScene->simulate(deltaTime);
    mScene->fetchResults(true);
}

int PhysXUnity::GetAllObjectTransforms(ObjectData* buffer, int bufferSize)
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

void PhysXUnity::SetObjectTransform(int index, float x, float y, float z, float qx, float qy, float qz, float qw, float vx, float vy, float vz)
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

int PhysXUnity::GetObjectCount() { return (int)gActors.size(); }

void PhysXUnity::ShutdownPhysics()
{
    // not solved yet, releasing causes Unity Editor crash
    DeleteAllObjects();
}