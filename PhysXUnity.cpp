#include <vector>
#include <thread>
#include <DirectXMath.h>
#include <PxPhysicsAPI.h>
#include <gpu/PxPhysicsGpu.h>
#include <cuda_runtime.h>
#include <Windows.h>
#include <string>
#include <algorithm>
#include <array>

#include "headers\PhysXUnity.h"
#include "headers\ObjectManager.h"

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
const char* CUDA_error = "N/A";

static bool CUDA_requested = false;
static bool DisplayErrors = false;

constexpr std::array<const char*, 37> dangerGPUs = {
    // 6.1
    "tesla p4", // also catches Tesla P40
    "quadro p6000",
    "quadro p5200",
    "quadro p5000",
    "quadro p4200",
    "quadro p4000",
    "quadro p3200",
    "quadro p3000",
    "quadro p2200",
    "quadro p2000",
    "quadro p1000",
	"quadro p620",
    "quadro p600",
    "quadro p500",
    "quadro p400",
	"titan x", // also catches Titan Xp & GTX TITAN X (5.2)
    "geforce gtx 1080", // includes Ti
    "geforce gtx 1070", // includes Ti
    "geforce gtx 1060",
    "geforce gtx 1050",

    // 6.0
    "tesla p100",
    "quadro gp100",
    
    // 5.2
    "tesla m60",
    "tesla m40",
    "quadro m6000",
    "quadro m5000",
    "quadro m4000",
    "quadro m2000",
    "quadro m5500m",
    "quadro m2200",
    "quadro m620",
    "geforce gtx 980", // includes Ti & 980M
    "geforce gtx 970", // includes 970M
    "geforce gtx 960",
    "geforce gtx 950",
    "geforce gtx 965m",
    "geforce 910m"
};

int solverIterations = 16;
int subStepTargetFPS = 60;

class MyErrorCallback : public PxErrorCallback
{
public:
    void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
        if (DisplayErrors)
        {
            char msg[512];
            snprintf(msg, 512, "PhysX Error %d: %s (%s:%d)\n", code, message, file, line);
            MessageBoxA(0, msg, "PhysX Error", MB_ICONERROR);
        }
    }
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

static PxScene* CreateScene()
{
    PxSceneDesc sceneDesc(mPhysics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;
    sceneDesc.maxNbContactDataBlocks = 3906250;

    CUDA = false;
    CUDA_error = "N/A";

    if (CUDA_requested)
    {
        int deviceCount = 0;
        cudaGetDeviceCount(&deviceCount);

        if (deviceCount > 0)
        {
            cudaDeviceProp deviceProp;
            cudaGetDeviceProperties(&deviceProp, 0);

            DisplayErrors = true;

            bool allowed = true;
            std::string gpuNameLower = toLower(deviceProp.name);
            for (const char* gpu : dangerGPUs)
            {
                if (gpuNameLower.find(gpu) != std::string::npos)
                {
                    char message[512];
                    snprintf(message, 512, "Your CUDA device (%s) does not meet the Compute Capability recommendation of 7.0!\n\nYou can try to enable CUDA anyway, but it's not officially supported by this PhysX version and might crash Sharpfall.\n\nTo enable anyway, press Yes.", deviceProp.name);
                    allowed = MessageBoxA(0, message, "libSharpfall Warning", MB_ICONWARNING | MB_YESNO) == IDYES;
                    break;
                }
            }

            if (allowed)
            {
                PxCudaContextManagerDesc desc;
                mCudaContextManager = PxCreateCudaContextManager(*mFoundation, desc, nullptr);

                if (mCudaContextManager && mCudaContextManager->contextIsValid())
                {
                    sceneDesc.cudaContextManager = mCudaContextManager;
                    sceneDesc.flags |= PxSceneFlag::eENABLE_GPU_DYNAMICS;
                    sceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
                    sceneDesc.flags |= PxSceneFlag::eDISABLE_CCD_RESWEEP;
                    sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU;

                    PxGpuDynamicsMemoryConfig gpuMem;
                    gpuMem.maxRigidContactCount = 1024 * 512 * 16;
                    gpuMem.maxRigidPatchCount = 1024 * 80 * 16;
                    gpuMem.foundLostPairsCapacity = 256 * 1024 * 16;
                    sceneDesc.gpuDynamicsConfig = gpuMem;

                    CUDA = true;
                    CUDA_device = _strdup(deviceProp.name);
                }
                else {
                    MessageBoxA(0, "Failed to create CUDA Context Manager, cannot enable CUDA acceleration.", "libSharpfall Error", MB_ICONERROR);
                    CUDA_error = "Failed to create CUDA Context Manager";
                }
            }
            
            DisplayErrors = false;
        }
        else
        {
            MessageBoxA(0, "No CUDA devices found, cannot enable CUDA acceleration.", "libSharpfall Error", MB_ICONERROR);
            CUDA_error = "No CUDA devices found";
        }
    }

    return mPhysics->createScene(sceneDesc);
}


extern "C"
{
    __declspec(dllexport) bool PXU_GetCUDAStatus() { return CUDA; }
    __declspec(dllexport) const char* PXU_GetCUDADevice() { return CUDA_device; }
    __declspec(dllexport) const char* PXU_GetCUDAError() { return CUDA_error; }

    __declspec(dllexport) int PXU_InitPhysics()
    {
        if (mScene)
        {
            //MessageBoxA(0, "Attempt to invoke PXU_InitPhysics when already initialized", "libSharpfall Warning", MB_ICONWARNING);
            return 1;
        }

        static PxDefaultAllocator gAllocator;
        //static PxDefaultErrorCallback gErrorCallback;
		static MyErrorCallback gMyErrorCallback;

        // Create foundation
        mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gMyErrorCallback);
        if (!mFoundation)
        {
            MessageBoxA(0, "Failed to initialize PhysX on: PxCreateFoundation", "libSharpfall Error", MB_ICONERROR);
            return 0;
        }

        // Create physics
        mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale());
        if (!mPhysics)
        {
            MessageBoxA(0, "Failed to initialize PhysX on: PxCreatePhysics", "libSharpfall Error", MB_ICONERROR);
            return 0;
        }

        // Create default material
        mMaterial = mPhysics->createMaterial(0.25f, 0.25f, 0.15f);

        mScene = CreateScene();
        if (!mScene)
        {
            MessageBoxA(0, "Failed to initialize PhysX on: mPhysics->createScene", "libSharpfall Error", MB_ICONERROR);
            return 0;
        }

        gActors.clear();

        // Create static platform
        PxTransform platformTransform(PxVec3(0.0f, -5.0f, 0.0f));
        PxBoxGeometry platformGeom(10.0f, 5.0f, 10.0f);
        PxMaterial* platformMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.10f);
        PxRigidStatic* platform = PxCreateStatic(*mPhysics, platformTransform, platformGeom, *platformMaterial);
        mScene->addActor(*platform);

        return 1;
    }

    __declspec(dllexport) int PXU_CreateObject(float x, float y, float z, float vx, float vy, float vz)
    {
        PxRigidDynamic* body = mPhysics->createRigidDynamic(PxTransform(PxVec3(x, y, z)));
        PxShape* shape = mPhysics->createShape(PxBoxGeometry(0.05f, 0.5f, 0.5f), *mMaterial);

        body->attachShape(*shape);

        body->setSolverIterationCounts(solverIterations * (CUDA ? 2 : 1), 1);

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

    __declspec(dllexport) void PXU_BeginStep(float deltaTime)
    {
        NewFrame();
        mScene->simulate(deltaTime);
    }

    __declspec(dllexport) bool PXU_IsStepDone()
    {
        return mScene->fetchResults(false);
    }

    __declspec(dllexport) void PXU_CompleteStep()
    {
        mScene->fetchResults(true);
    }

    __declspec(dllexport) void PXU_SignalNewFrame()
    {
        NewFrame();
    }

    __declspec(dllexport) void PXU_StepPhysics(float deltaTime)
    {
        mScene->simulate(deltaTime);
        mScene->fetchResults(true);
    }

    __declspec(dllexport) int PXU_GetAllObjectMatrices(UnityMatrix* buffer, int bufferSize)
    {
        int count = min(bufferSize, (int)gActors.size());
        for (int i = 0; i < count; i++)
        {
            PxTransform t = gActors[i]->getGlobalPose();
            PxVec3 s(0.1f, 1.0f, 1.0f); // scale

            PxQuat q = t.q;

            // Precompute quaternion products
            float xx = q.x * q.x;
            float yy = q.y * q.y;
            float zz = q.z * q.z;
            float xy = q.x * q.y;
            float xz = q.x * q.z;
            float yz = q.y * q.z;
            float wx = q.w * q.x;
            float wy = q.w * q.y;
            float wz = q.w * q.z;

            // Column-major rotation * scale for Unity
            buffer[i].m00 = (1 - 2 * (yy + zz)) * s.x;
            buffer[i].m01 = (2 * (xy + wz)) * s.x;
            buffer[i].m02 = (2 * (xz - wy)) * s.x;
            buffer[i].m03 = 0.0f;

            buffer[i].m10 = (2 * (xy - wz)) * s.y;
            buffer[i].m11 = (1 - 2 * (xx + zz)) * s.y;
            buffer[i].m12 = (2 * (yz + wx)) * s.y;
            buffer[i].m13 = 0.0f;

            buffer[i].m20 = (2 * (xz + wy)) * s.z;
            buffer[i].m21 = (2 * (yz - wx)) * s.z;
            buffer[i].m22 = (1 - 2 * (xx + yy)) * s.z;
            buffer[i].m23 = 0.0f;

            buffer[i].m30 = t.p.x;
            buffer[i].m31 = t.p.y;
            buffer[i].m32 = t.p.z;
            buffer[i].m33 = 1.0f;
        }
        return count;
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

        if (mScene)
        {
            mScene->release();
            mScene = nullptr;
        }

        if (mCudaContextManager)
        {
            mCudaContextManager->release();
            mCudaContextManager = nullptr;
        }
    }

    __declspec(dllexport) bool PXU_SetCUDAState(bool enabled)
    {
        if (CUDA == enabled)
            return true;

        CUDA_requested = enabled;

        if (!mPhysics)
            return false;

        // Scene must not be simulating here
        PXU_ShutdownPhysics();

        mScene = CreateScene();
        if (!mScene)
        {
            MessageBoxA(0, "Failed to initialize PhysX on: mPhysics->createScene", "libSharpfall Error", MB_ICONERROR);
            return false;
        }

        gActors.clear();

        // Create static platform
        PxTransform platformTransform(PxVec3(0.0f, -5.0f, 0.0f));
        PxBoxGeometry platformGeom(10.0f, 5.0f, 10.0f);
        PxMaterial* platformMaterial = mPhysics->createMaterial(0.5f, 0.5f, 0.10f);
        PxRigidStatic* platform = PxCreateStatic(*mPhysics, platformTransform, platformGeom, *platformMaterial);
        mScene->addActor(*platform);

        return true;
    }

    __declspec(dllexport) void PXU_SetSolverIterations(int value)
    {
        if (value <= 0)
            return;

        solverIterations = value;
        PXU_DeleteAllObjects();
    }
}

int PhysXUnity::InitPhysics()
{
    return PXU_InitPhysics();
}
int PhysXUnity::CreateObject(float x, float y, float z, float vx, float vy, float vz)
{
    return PXU_CreateObject(x, y, z, vx, vy, vz);
}
void PhysXUnity::DeleteAllObjects()
{
    PXU_DeleteAllObjects();
}
void PhysXUnity::BeginStep(float deltaTime)
{
    PXU_BeginStep(deltaTime);
}
void PhysXUnity::IsStepDone()
{
    PXU_IsStepDone();
}
void PhysXUnity::CompleteStep()
{
    PXU_CompleteStep();
}
void PhysXUnity::StepPhysics(float deltaTime)
{
    PXU_StepPhysics(deltaTime);
}
int PhysXUnity::GetAllObjectMatrices(UnityMatrix* buffer, int bufferSize)
{
    return PXU_GetAllObjectMatrices(buffer, bufferSize);
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