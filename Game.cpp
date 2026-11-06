#include "Game.h"

#pragma warning(push)
#pragma warning(disable:4023)

#include <Windows.h>
#include "Math.h"
#include "ModelData.h"
#include "Audio.h"
#include "Input.h"
#include "DebugCamera.h"
#include "Camera.h"
#include "BlendMode.h"
#include "ParticleSystem.h"
#include "ParticleManager.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"

#include <wrl.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <vector>
#include <numbers>
#include <format>
#include <ostream>
#include <utility>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma warning(pop)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

// =====================================
// クラッシュダンプ
// =====================================
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    SYSTEMTIME time;
    GetLocalTime(&time);

    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);

    StringCchPrintf(
        filePath,
        MAX_PATH,
        L"./Dumps/%04d-%02d%02d-%02d%02d%02d.dmp",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond
    );

    HANDLE dumpFileHandle = CreateFile(
        filePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        0,
        CREATE_ALWAYS,
        0,
        0
    );

    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    MiniDumpWriteDump(
        GetCurrentProcess(),
        processId,
        dumpFileHandle,
        MiniDumpNormal,
        &minidumpInformation,
        nullptr,
        nullptr
    );

    CloseHandle(dumpFileHandle);

    return EXCEPTION_EXECUTE_HANDLER;
}

// =====================================
// Gameクラスの内部データ
// =====================================
struct Game::Impl {
    D3DResourceLeakChecker leakChecker;
    Audio audio;

    Input* input = nullptr;
    DebugCamera debugCamera;
    Camera camera;

    bool useDebugCamera = true;
    Vector3 cameraPosition{};
    Vector3 cameraRotation{};

    WinApp winApp;

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
#endif

    DirectXCommon dxCommon;
    SrvManager srvManager;
    ImGuiManager imguiManager;

    ID3D12GraphicsCommandList* commandList = nullptr;

    ParticleManager particleManager;
    BlendMode blendMode = kBlendModeNormal;

    Emitter emitter{};
    Emitter monsterEmitter{};

    bool useBillboard = true;
    bool useAccelerationField = true;

    Transform particleUVTransform{};
    const float kDeltaTime = 1.0f / 60.0f;
    Transform transform{};

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResourceOwner;
    ID3D12Resource* cameraResource = nullptr;
    CameraForGPU* cameraData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResourceOwner;
    ID3D12Resource* directionalLightResource = nullptr;
    DirectionalLight* directionalLightData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResourceOwner;
    ID3D12Resource* pointLightResource = nullptr;
    PointLight* pointLightData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResourceOwner;
    ID3D12Resource* spotLightResource = nullptr;
    SpotLight* spotLightData = nullptr;

    float spotLightAngle = 60.0f;
    float spotLightFalloffStartAngle = 30.0f;

    bool useDirectionalLight = true;
    bool usePointLight = true;

    float directionalLightIntensity = 1.0f;
    float pointLightIntensity = 1.0f;

    TextureManager textureManager;

    uint32_t circleTextureIndex = 0;
    uint32_t monsterTextureIndex = 0;

    bool useMonsterBall = true;

    Object3dCommon object3dCommon;
    ModelManager modelManager;

    Model* sphereModel = nullptr;
    Material* materialData = nullptr;

    Object3d sphereObject;
    Object3d object1;
    Object3d object2;

    SpriteCommon spriteCommon;
    Sprite sprite1;
    Sprite sprite2;

    bool isSpriteTextureUploadReleased = false;

    void Initialize();
    void Update();
    void Draw();
    void Finalize();
};

// =====================================
// 初期化
// =====================================
void Game::Impl::Initialize() {
    CoInitializeEx(0, COINIT_MULTITHREADED);
    SetUnhandledExceptionFilter(ExportDump);

    input = new Input();

    debugCamera.Initialize();
    camera.Initialize();

    useDebugCamera = true;

    cameraPosition = {
        0.0f, 0.0f, -50.0f
    };

    cameraRotation = {
        0.0f, 0.0f, 0.0f
    };

    winApp.Initialize();

#ifdef _DEBUG
    if (SUCCEEDED(D3D12GetDebugInterface(
        IID_PPV_ARGS(debugController.GetAddressOf())
    ))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    Logger::Initialize();

    dxCommon.Initialize(&winApp);
    srvManager.Initialize(&dxCommon);

    imguiManager.Initialize(
        &winApp,
        &dxCommon,
        &srvManager
    );

    ID3D12Device* device = dxCommon.GetDevice();

    input->Initialize(
        winApp.GetHInstance(),
        winApp.GetHwnd()
    );

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

    if (SUCCEEDED(device->QueryInterface(
        IID_PPV_ARGS(infoQueue.GetAddressOf())
    ))) {
        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_CORRUPTION,
            true
        );

        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_ERROR,
            true
        );

        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_WARNING,
            true
        );

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };

        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_INFO_QUEUE_FILTER filter{};

        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;

        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;

        infoQueue->PushStorageFilter(&filter);
        infoQueue.Reset();
    }
#endif

    commandList = dxCommon.GetCommandList();

    // 音声
    audio.Initialize();
    audio.LoadSound("resources/Alarm01.wav");
    audio.PlayWave();

    // Particle
    particleManager.Initialize(
        &dxCommon,
        &srvManager
    );

    blendMode = kBlendModeNormal;

    AccelerationField accelerationField{};

    accelerationField.acceleration = {
        15.0f, 0.0f, 0.0f
    };

    accelerationField.area.min = {
        -1.0f, -1.0f, -1.0f
    };

    accelerationField.area.max = {
        1.0f, 1.0f, 1.0f
    };

    particleManager.SetAccelerationField(
        accelerationField
    );

    emitter = {};

    emitter.transform.scale = {
        1.0f, 1.0f, 1.0f
    };

    emitter.transform.rotate = {
        0.0f, 0.0f, 0.0f
    };

    emitter.transform.translate = {
        0.0f, 0.0f, 0.0f
    };

    emitter.count = 3;
    emitter.frequency = 0.5f;
    emitter.frequencyTime = 0.0f;

    useBillboard = true;
    useAccelerationField = true;

    particleUVTransform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    // 球の頂点生成
    ModelData modelData{};

    const uint32_t kSubdivision = 16;

    const float kLonEvery =
        std::numbers::pi_v<float> *2.0f /
        static_cast<float>(kSubdivision);

    const float kLatEvery =
        std::numbers::pi_v<float> /
        static_cast<float>(kSubdivision);

    modelData.vertices.resize(
        kSubdivision * kSubdivision * 6
    );

    for (uint32_t latIndex = 0;
        latIndex < kSubdivision;
        ++latIndex) {

        float lat =
            -std::numbers::pi_v<float> / 2.0f +
            kLatEvery * static_cast<float>(latIndex);

        float nextLat = lat + kLatEvery;

        for (uint32_t lonIndex = 0;
            lonIndex < kSubdivision;
            ++lonIndex) {

            float lon =
                kLonEvery * static_cast<float>(lonIndex);

            float nextLon = lon + kLonEvery;

            uint32_t start =
                (latIndex * kSubdivision + lonIndex) * 6;

            Vector4 a = {
                std::cos(lat) * std::cos(lon),
                std::sin(lat),
                std::cos(lat) * std::sin(lon),
                1.0f
            };

            Vector4 b = {
                std::cos(nextLat) * std::cos(lon),
                std::sin(nextLat),
                std::cos(nextLat) * std::sin(lon),
                1.0f
            };

            Vector4 c = {
                std::cos(lat) * std::cos(nextLon),
                std::sin(lat),
                std::cos(lat) * std::sin(nextLon),
                1.0f
            };

            Vector4 d = {
                std::cos(nextLat) * std::cos(nextLon),
                std::sin(nextLat),
                std::cos(nextLat) * std::sin(nextLon),
                1.0f
            };

            float u0 =
                static_cast<float>(lonIndex) /
                static_cast<float>(kSubdivision);

            float u1 =
                static_cast<float>(lonIndex + 1) /
                static_cast<float>(kSubdivision);

            float v0 =
                1.0f -
                static_cast<float>(latIndex) /
                static_cast<float>(kSubdivision);

            float v1 =
                1.0f -
                static_cast<float>(latIndex + 1) /
                static_cast<float>(kSubdivision);

            modelData.vertices[start + 0] = {
                .position = a,
                .texcoord = {u0, v0},
                .normal = {a.x, a.y, a.z}
            };

            modelData.vertices[start + 1] = {
                .position = b,
                .texcoord = {u0, v1},
                .normal = {b.x, b.y, b.z}
            };

            modelData.vertices[start + 2] = {
                .position = c,
                .texcoord = {u1, v0},
                .normal = {c.x, c.y, c.z}
            };

            modelData.vertices[start + 3] = {
                .position = c,
                .texcoord = {u1, v0},
                .normal = {c.x, c.y, c.z}
            };

            modelData.vertices[start + 4] = {
                .position = b,
                .texcoord = {u0, v1},
                .normal = {b.x, b.y, b.z}
            };

            modelData.vertices[start + 5] = {
                .position = d,
                .texcoord = {u1, v1},
                .normal = {d.x, d.y, d.z}
            };
        }
    }

    assert(!modelData.vertices.empty());

    transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, -1.54f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    // Camera用リソース
    cameraResourceOwner =
        dxCommon.CreateBufferResource(256);

    cameraResource = cameraResourceOwner.Get();

    cameraResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&cameraData)
    );

    cameraData->worldPosition =
        debugCamera.GetWorldPosition();

    cameraData->padding = 0.0f;

    // 平行光源
    directionalLightResourceOwner =
        dxCommon.CreateBufferResource(
            sizeof(DirectionalLight)
        );

    directionalLightResource =
        directionalLightResourceOwner.Get();

    directionalLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&directionalLightData)
    );

    directionalLightData->color =
    { 1.0f, 1.0f, 1.0f, 1.0f };

    directionalLightData->direction =
    { 0.0f, -1.0f, 0.0f };

    directionalLightData->intensity = 1.0f;

    // 点光源
    pointLightResourceOwner =
        dxCommon.CreateBufferResource(
            sizeof(PointLight)
        );

    pointLightResource =
        pointLightResourceOwner.Get();

    pointLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&pointLightData)
    );

    pointLightData->color =
    { 1.0f, 1.0f, 1.0f, 1.0f };

    pointLightData->position =
    { 0.0f, 2.0f, 0.0f };

    pointLightData->intensity = 1.0f;
    pointLightData->radius = 10.0f;
    pointLightData->decay = 1.0f;
    pointLightData->padding[0] = 0.0f;
    pointLightData->padding[1] = 0.0f;

    // スポットライト
    spotLightResourceOwner =
        dxCommon.CreateBufferResource(
            sizeof(SpotLight)
        );

    spotLightResource = spotLightResourceOwner.Get();

    spotLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&spotLightData)
    );

    spotLightData->color =
    { 1.0f, 1.0f, 1.0f, 1.0f };

    spotLightData->position =
    { 2.0f, 1.25f, 0.0f };

    spotLightData->intensity = 4.0f;

    spotLightData->direction =
    { -1.0f, -1.0f, 0.0f };

    spotLightData->distance = 7.0f;
    spotLightData->decay = 2.0f;

    spotLightData->cosAngle =
        std::cos(std::numbers::pi_v<float> / 3.0f);

    spotLightData->cosFalloffStart =
        std::cos(std::numbers::pi_v<float> / 6.0f);

    spotLightAngle = 60.0f;
    spotLightFalloffStartAngle = 30.0f;

    spotLightData->padding[0] = 0.0f;
    spotLightData->padding[1] = 0.0f;

    useDirectionalLight = true;
    usePointLight = true;

    directionalLightIntensity = 1.0f;
    pointLightIntensity = 1.0f;

    // TextureManager
    textureManager.Initialize(
        &dxCommon,
        &srvManager
    );

    circleTextureIndex =
        textureManager.LoadTexture(
            "resources/circle.png"
        );

    monsterTextureIndex =
        textureManager.LoadTexture(
            "resources/monsterBall.png"
        );

    useMonsterBall = true;

    // Particle用テクスチャ
    particleManager.SetTextureHandle(
        textureManager.GetGPUHandle(
            circleTextureIndex
        )
    );

    particleManager.CreateParticleGroup(
        "monster",
        textureManager.GetGPUHandle(
            monsterTextureIndex
        )
    );

    monsterEmitter = emitter;

    monsterEmitter.transform.translate = {
        4.0f, 0.0f, 0.0f
    };

    monsterEmitter.frequencyTime = 0.0f;

    // Object3d
    object3dCommon.Initialize(&dxCommon);
    object3dCommon.SetDefaultCamera(&camera);

    // ModelManager
    modelManager.Initialize(
        &dxCommon,
        &textureManager
    );

    sphereModel =
        modelManager.RegisterGeneratedModel(
            "sphere",
            std::move(modelData),
            circleTextureIndex
        );

    assert(sphereModel != nullptr);

    materialData = sphereModel->GetMaterialData();
    assert(materialData != nullptr);

    sphereObject.Initialize(&object3dCommon);
    sphereObject.SetModel(sphereModel);
    sphereObject.SetTransform(transform);

    // plane
    Model* planeModel = modelManager.LoadModel(
        "resources/plane",
        "plane.gltf"
    );

    assert(planeModel != nullptr);

    // fence
    Model* secondModel = modelManager.LoadModel(
        "resources/fence",
        "fence.obj"
    );

    assert(secondModel != nullptr);
    assert(planeModel != secondModel);

    object1.Initialize(&object3dCommon);
    object1.SetModel(planeModel);
    object1.SetPosition({ 3.0f, 0.0f, 0.0f });
    object1.SetRotation({ 0.0f, -1.54f, 0.0f });
    object1.SetScale({ 1.0f, 1.0f, 1.0f });

    object2.Initialize(&object3dCommon);
    object2.SetModel(secondModel);
    object2.SetPosition({ 6.0f, 0.0f, 0.0f });
    object2.SetRotation({ 0.0f, -1.54f, 0.0f });
    object2.SetScale({ 1.0f, 1.0f, 1.0f });

    // SpriteCommon
    spriteCommon.Initialize(&dxCommon);

    sprite1.Initialize(
        &spriteCommon,
        textureManager.GetGPUHandle(circleTextureIndex),
        textureManager.GetTextureSize(circleTextureIndex)
    );

    sprite1.SetPosition({ 50.0f, 50.0f });
    sprite1.SetSize({ 200.0f, 200.0f });

    sprite2.Initialize(
        &spriteCommon,
        textureManager.GetGPUHandle(monsterTextureIndex),
        textureManager.GetTextureSize(monsterTextureIndex)
    );

    sprite2.SetPosition({ 350.0f, 150.0f });
    sprite2.SetSize({ 200.0f, 200.0f });

    Vector2 monsterTextureSize =
        textureManager.GetTextureSize(monsterTextureIndex);

    sprite2.SetTextureLeftTop({ 0.0f, 0.0f });

    sprite2.SetTextureSize({
        monsterTextureSize.x * 0.5f,
        monsterTextureSize.y * 0.5f
        });

    isSpriteTextureUploadReleased = false;
}

// =====================================
// 更新
// =====================================
void Game::Impl::Update() {
    imguiManager.Begin();

    input->Update();

    if (input->PushKey(DIK_0)) {
        OutputDebugStringA("Hit 0\n");
    }

    bool isImGuiUsingMouse = false;

#ifdef USE_IMGUI
    isImGuiUsingMouse =
        ImGui::GetIO().WantCaptureMouse;
#endif

    // Camera更新
    if (useDebugCamera) {
        if (!isImGuiUsingMouse) {
            debugCamera.Update(input);
        }

        camera.SetViewMatrix(
            debugCamera.GetViewMatrix()
        );
    }
    else {
        camera.SetTranslate(cameraPosition);
        camera.SetRotate(cameraRotation);
        camera.Update();
    }

    cameraData->worldPosition =
        camera.GetTranslate();

    Matrix4x4 viewMatrix =
        camera.GetViewMatrix();

    Matrix4x4 projectionMatrix =
        camera.GetProjectionMatrix();

    // Object更新
    sphereObject.SetTransform(transform);
    sphereObject.Update();
    object1.Update();
    object2.Update();

    // Billboard計算
    Matrix4x4 backToFrontMatrix =
        MakeRotateYMatrix(
            std::numbers::pi_v<float>
        );

    Matrix4x4 cameraMatrix = MakeIdentity4x4();

    cameraMatrix.m[0][0] = viewMatrix.m[0][0];
    cameraMatrix.m[0][1] = viewMatrix.m[1][0];
    cameraMatrix.m[0][2] = viewMatrix.m[2][0];

    cameraMatrix.m[1][0] = viewMatrix.m[0][1];
    cameraMatrix.m[1][1] = viewMatrix.m[1][1];
    cameraMatrix.m[1][2] = viewMatrix.m[2][1];

    cameraMatrix.m[2][0] = viewMatrix.m[0][2];
    cameraMatrix.m[2][1] = viewMatrix.m[1][2];
    cameraMatrix.m[2][2] = viewMatrix.m[2][2];

    Matrix4x4 billboardMatrix = Multiply(
        backToFrontMatrix,
        cameraMatrix
    );

    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    Matrix4x4 viewProjectionMatrix = Multiply(
        viewMatrix,
        projectionMatrix
    );

    // Particle更新
    emitter.frequencyTime += kDeltaTime;

    if (emitter.frequency <= emitter.frequencyTime) {
        particleManager.Emit(emitter);
        emitter.frequencyTime -= emitter.frequency;
    }

    monsterEmitter.frequencyTime += kDeltaTime;

    if (monsterEmitter.frequency <=
        monsterEmitter.frequencyTime) {

        particleManager.Emit(
            "monster",
            monsterEmitter
        );

        monsterEmitter.frequencyTime -=
            monsterEmitter.frequency;
    }

    particleManager.Update(
        viewProjectionMatrix,
        billboardMatrix,
        kDeltaTime,
        useBillboard,
        useAccelerationField
    );

    // UVアニメーション
    particleUVTransform.translate.x +=
        0.2f * kDeltaTime;

    if (particleUVTransform.translate.x >= 1.0f) {
        particleUVTransform.translate.x -= 1.0f;
    }

    Matrix4x4 particleUVMatrix =
        MakeScaleMatrix(particleUVTransform.scale);

    particleUVMatrix = Multiply(
        particleUVMatrix,
        MakeRotateZMatrix(
            particleUVTransform.rotate.z
        )
    );

    particleUVMatrix = Multiply(
        particleUVMatrix,
        MakeTranslateMatrix(
            particleUVTransform.translate
        )
    );

    particleManager.SetUVTransform(
        particleUVMatrix
    );

    sprite1.Update();
    sprite2.Update();

#ifdef USE_IMGUI
    ImGui::Begin("Material");

    ImGui::DragFloat3(
        "SphereScale",
        &transform.scale.x,
        0.01f,
        0.01f,
        10.0f
    );

    ImGui::DragFloat3(
        "ModelRotate",
        &transform.rotate.x,
        0.01f
    );

    ImGui::ColorEdit4(
        "Color",
        &materialData->color.x
    );

    const char* blendModeNames[] = {
        "kBlendModeNone",
        "kBlendModeNormal",
        "kBlendModeAdd",
        "kBlendModeSub",
        "kBlendModeMul",
        "kBlendModeScreen"
    };

    int blendModeIndex =
        static_cast<int>(blendMode);

    if (ImGui::Combo(
        "Blend",
        &blendModeIndex,
        blendModeNames,
        kCountOfBlendMode
    )) {
        blendMode =
            static_cast<BlendMode>(blendModeIndex);
    }

    ImGui::Checkbox(
        "useMonsterBall",
        &useMonsterBall
    );

    // DirectionalLight
    ImGui::ColorEdit4(
        "Light Color",
        &directionalLightData->color.x
    );

    if (ImGui::DragFloat3(
        "Light Direction",
        &directionalLightData->direction.x,
        0.01f
    )) {
        directionalLightData->direction =
            Normalize(directionalLightData->direction);
    }

    ImGui::DragFloat(
        "DirectionalLight Intensity",
        &directionalLightIntensity,
        0.01f, 0.0f, 10.0f
    );

    ImGui::DragFloat(
        "Shininess",
        &materialData->shininess,
        1.0f, 1.0f, 256.0f
    );

    ImGui::Checkbox(
        "DirectionalLight",
        &useDirectionalLight
    );

    // PointLight
    ImGui::ColorEdit4(
        "PointLight Color",
        &pointLightData->color.x
    );

    ImGui::DragFloat3(
        "PointLight Position",
        &pointLightData->position.x,
        0.01f, -10.0f, 10.0f
    );

    ImGui::DragFloat(
        "PointLight Intensity",
        &pointLightIntensity,
        0.01f, 0.0f, 10.0f
    );

    ImGui::DragFloat(
        "PointLight Radius",
        &pointLightData->radius,
        0.1f, 0.1f, 100.0f
    );

    ImGui::DragFloat(
        "PointLight Decay",
        &pointLightData->decay,
        0.01f, 0.1f, 10.0f
    );

    ImGui::Checkbox(
        "PointLight",
        &usePointLight
    );

    // SpotLight
    ImGui::ColorEdit4(
        "SpotLight Color",
        &spotLightData->color.x
    );

    ImGui::DragFloat3(
        "SpotLight Position",
        &spotLightData->position.x,
        0.01f, -20.0f, 20.0f
    );

    if (ImGui::DragFloat3(
        "SpotLight Direction",
        &spotLightData->direction.x,
        0.01f
    )) {
        spotLightData->direction =
            Normalize(spotLightData->direction);
    }

    ImGui::DragFloat(
        "SpotLight Intensity",
        &spotLightData->intensity,
        0.01f, 0.0f, 10.0f
    );

    ImGui::DragFloat(
        "SpotLight Distance",
        &spotLightData->distance,
        0.1f, 0.1f, 100.0f
    );

    ImGui::DragFloat(
        "SpotLight Decay",
        &spotLightData->decay,
        0.01f, 0.1f, 10.0f
    );

    ImGui::DragFloat(
        "SpotLight Angle",
        &spotLightAngle,
        1.0f, 1.0f, 179.0f
    );

    ImGui::DragFloat(
        "SpotLight FalloffStart",
        &spotLightFalloffStartAngle,
        1.0f, 0.0f,
        spotLightAngle - 0.1f
    );

    spotLightData->cosAngle = std::cos(
        spotLightAngle *
        std::numbers::pi_v<float> / 180.0f
    );

    spotLightData->cosFalloffStart = std::cos(
        spotLightFalloffStartAngle *
        std::numbers::pi_v<float> / 180.0f
    );

    // Camera操作
    ImGui::SeparatorText("Camera");

    ImGui::Checkbox(
        "Use DebugCamera",
        &useDebugCamera
    );

    if (!useDebugCamera) {
        ImGui::DragFloat3(
            "Camera Position",
            &cameraPosition.x,
            0.1f
        );

        ImGui::DragFloat3(
            "Camera Rotation",
            &cameraRotation.x,
            0.01f
        );
    }

    ImGui::End();

    if (useDirectionalLight) {
        directionalLightData->intensity =
            directionalLightIntensity;
    }
    else {
        directionalLightData->intensity = 0.0f;
    }

    if (usePointLight) {
        pointLightData->intensity =
            pointLightIntensity;
    }
    else {
        pointLightData->intensity = 0.0f;
    }

    imguiManager.End();
#endif
}

// =====================================
// 描画
// =====================================
void Game::Impl::Draw() {
    if (winApp.ProcessMessage()) {
        return;
    }

    dxCommon.PreDraw();

    object3dCommon.CommonDraw(blendMode);

    // ライトのConstantBuffer
    commandList->SetGraphicsRootConstantBufferView(
        3,
        directionalLightResource->GetGPUVirtualAddress()
    );

    // カメラ
    commandList->SetGraphicsRootConstantBufferView(
        4,
        cameraResource->GetGPUVirtualAddress()
    );

    // 点光源
    commandList->SetGraphicsRootConstantBufferView(
        5,
        pointLightResource->GetGPUVirtualAddress()
    );

    // スポットライト
    commandList->SetGraphicsRootConstantBufferView(
        6,
        spotLightResource->GetGPUVirtualAddress()
    );

    sphereModel->SetTextureIndex(
        useMonsterBall
        ? monsterTextureIndex
        : circleTextureIndex
    );

    sphereObject.Draw();

    object1.Draw();
    object2.Draw();

    particleManager.Draw();

    spriteCommon.CommonDraw();

    sprite1.Draw();
    sprite2.Draw();

    imguiManager.Draw();

    dxCommon.PostDraw();

    if (!isSpriteTextureUploadReleased) {
        textureManager.ReleaseUploadResources();
        isSpriteTextureUploadReleased = true;
    }
}

// =====================================
// 終了処理
// =====================================
void Game::Impl::Finalize() {
    cameraResourceOwner.Reset();
    directionalLightResourceOwner.Reset();
    pointLightResourceOwner.Reset();
    spotLightResourceOwner.Reset();

    particleManager.Finalize();

    sphereObject.Finalize();
    object1.Finalize();
    object2.Finalize();

    modelManager.Finalize();
    object3dCommon.Finalize();

    textureManager.Finalize();
    imguiManager.Finalize();
    srvManager.Finalize();

    dxCommon.Finalize();

#ifdef _DEBUG
    debugController.Reset();
#endif

    winApp.Finalize();

    delete input;
    input = nullptr;

    audio.Finalize();
    Logger::Finalize();

    CoUninitialize();
}

// =====================================
// Gameクラス本体
// =====================================
Game::Game()
    : impl_(std::make_unique<Impl>()) {}

Game::~Game() = default;

void Game::Initialize() {
    impl_->Initialize();
}

void Game::Update() {
    impl_->Update();
}

void Game::Draw() {
    impl_->Draw();
}

void Game::Finalize() {
    impl_->Finalize();
}

bool Game::IsEndRequested() const {
    return impl_->winApp.IsEndRequested();
}