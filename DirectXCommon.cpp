#include "DirectXCommon.h"
#include "WinApp.h"
#include "ConvertString.h"
#include "Logger.h"
#include "externals/DirectXTex/d3dx12.h"

#include <vector>
#include <format>
#include <cassert>
#include <cstddef>
#include <thread>
#include <dxcapi.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
#endif

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// =====================================
// 全体の初期化
// =====================================
void DirectXCommon::Initialize(WinApp* winApp) {

    assert(winApp != nullptr);

    HWND hwnd = winApp->GetHwnd();
    assert(hwnd != nullptr);

    // FPS固定の初期化
    InitializeFixFPS();
    // デバイスの初期化
    InitializeDevice();
    // コマンド関連の初期化
    InitializeCommand();
    // スワップチェーンの生成
    CreateSwapChain(hwnd);
    // ディスクリプタヒープの生成
    CreateDescriptorHeaps();
    // RTVの生成
    InitializeRenderTargetView();
    // 深度バッファ生成
    CreateDepthBuffer();
    // DSVの生成
    InitializeDepthStencilView();
    // Fence初期化
    InitializeFence();
    // ビューポート初期化
    InitializeViewport();
    // シザー矩形初期化
    InitializeScissorRect();
    // DXCの初期化
    InitializeDXC();
    // ImGuiの初期化
    InitializeImGui(hwnd);
}

// =====================================
// デバイス初期化
// =====================================
void DirectXCommon::InitializeDevice() {

    HRESULT hr = CreateDXGIFactory(
        IID_PPV_ARGS(dxgiFactory_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    // GPUアダプターの選択
    for (UINT i = 0;; ++i) {

        Microsoft::WRL::ComPtr<IDXGIAdapter4> candidate;

        hr = dxgiFactory_->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(candidate.GetAddressOf())
        );

        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        assert(SUCCEEDED(hr));

        DXGI_ADAPTER_DESC3 desc{};
        hr = candidate->GetDesc3(&desc);
        assert(SUCCEEDED(hr));

        if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            useAdapter_ = candidate;
            break;
        }
    }

    assert(useAdapter_ != nullptr);

    // FeatureLevelを高い順に試す
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0
    };

    const char* featureLevelStrings[] = {
        "12.2",
        "12.1",
        "12.0"
    };

    for (size_t i = 0; i < _countof(featureLevels); ++i) {

        hr = D3D12CreateDevice(
            useAdapter_.Get(),
            featureLevels[i],
            IID_PPV_ARGS(device_.GetAddressOf())
        );

        if (SUCCEEDED(hr)) {

            Logger::Log(std::format(
                "FeatureLevel: {}",
                featureLevelStrings[i]
            ));

            break;
        }
    }

    assert(device_ != nullptr);

    Logger::Log("Complete create D3D12Device!!!");
}

// =====================================
// コマンド関連の初期化
// =====================================
void DirectXCommon::InitializeCommand() {

    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};

    hr = device_->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(commandQueue_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(commandAllocator_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(commandList_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));
}

// =====================================
// スワップチェーン生成
// =====================================
void DirectXCommon::CreateSwapChain(HWND hwnd) {

    DXGI_SWAP_CHAIN_DESC1 desc{};

    desc.Width = WinApp::kClientWidth;
    desc.Height = WinApp::kClientHeight;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBackBufferCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;

    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &desc,
        nullptr,
        nullptr,
        tempSwapChain.GetAddressOf()
    );
    assert(SUCCEEDED(hr));

    hr = tempSwapChain.As(&swapChain_);
    assert(SUCCEEDED(hr));
}

// =====================================
// ディスクリプタヒープの生成
// =====================================
void DirectXCommon::CreateDescriptorHeaps() {

    descriptorSizeRTV_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        );

    descriptorSizeSRV_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );

    descriptorSizeDSV_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV
        );

    HRESULT hr;

    // RTV
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = kBackBufferCount;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device_->CreateDescriptorHeap(
        &rtvDesc,
        IID_PPV_ARGS(rtvDescriptorHeap_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    // SRV
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 128;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = device_->CreateDescriptorHeap(
        &srvDesc,
        IID_PPV_ARGS(srvDescriptorHeap_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device_->CreateDescriptorHeap(
        &dsvDesc,
        IID_PPV_ARGS(dsvDescriptorHeap_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));
}

// =====================================
// バックバッファとRTVの生成
// =====================================
void DirectXCommon::InitializeRenderTargetView() {

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    for (UINT i = 0; i < kBackBufferCount; ++i) {

        HRESULT hr = swapChain_->GetBuffer(
            i,
            IID_PPV_ARGS(swapChainResources_[i].GetAddressOf())
        );
        assert(SUCCEEDED(hr));

        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

        handle.ptr += static_cast<SIZE_T>(descriptorSizeRTV_) * i;

        rtvHandles_[i] = handle;

        device_->CreateRenderTargetView(
            swapChainResources_[i].Get(),
            &rtvDesc,
            handle
        );
    }
}

// =====================================
// 深度バッファ生成
// =====================================
void DirectXCommon::CreateDepthBuffer() {

    D3D12_RESOURCE_DESC desc{};

    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = WinApp::kClientWidth;
    desc.Height = WinApp::kClientHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(depthStencilResource_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// DSV生成
// =====================================
void DirectXCommon::InitializeDepthStencilView() {

    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device_->CreateDepthStencilView(
        depthStencilResource_.Get(),
        &desc,
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart()
    );
}

// =====================================
// Fence初期化
// =====================================
void DirectXCommon::InitializeFence() {

    fenceValue_ = 0;

    HRESULT hr = device_->CreateFence(
        fenceValue_,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(fence_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    fenceEvent_ = CreateEvent(
        nullptr,
        FALSE,
        FALSE,
        nullptr
    );

    assert(fenceEvent_ != nullptr);
}

// =====================================
// ビューポート設定
// =====================================
void DirectXCommon::InitializeViewport() {

    viewport_.Width =
        static_cast<float>(WinApp::kClientWidth);

    viewport_.Height =
        static_cast<float>(WinApp::kClientHeight);

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

// =====================================
// シザー矩形設定
// =====================================
void DirectXCommon::InitializeScissorRect() {

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

// =====================================
// SRV用CPUハンドル
// =====================================
D3D12_CPU_DESCRIPTOR_HANDLE
DirectXCommon::GetSRVCPUDescriptorHandle(
    uint32_t index) const {

    assert(index < 128);

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    handle.ptr += static_cast<SIZE_T>(descriptorSizeSRV_) * index;

    return handle;
}

// =====================================
// SRV用GPUハンドル
// =====================================
D3D12_GPU_DESCRIPTOR_HANDLE
DirectXCommon::GetSRVGPUDescriptorHandle(
    uint32_t index) const {

    assert(index < 128);

    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    handle.ptr += static_cast<UINT64>(descriptorSizeSRV_) * index;

    return handle;
}

// =====================================
// 描画前処理
// =====================================
void DirectXCommon::PreDraw() {

    UINT backBufferIndex =
        swapChain_->GetCurrentBackBufferIndex();

    assert(backBufferIndex < kBackBufferCount);

    // Present → RenderTarget
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource =
        swapChainResources_[backBufferIndex].Get();
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 描画先設定
    commandList_->OMSetRenderTargets(
        1,
        &rtvHandles_[backBufferIndex],
        FALSE,
        &dsvHandle
    );

    // 画面クリア
    const float clearColor[] = {
        0.1f, 0.25f, 0.5f, 1.0f
    };

    commandList_->ClearRenderTargetView(
        rtvHandles_[backBufferIndex],
        clearColor,
        0,
        nullptr
    );

    // 深度クリア
    commandList_->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );

    // SRVヒープの設定
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvDescriptorHeap_.Get()
    };

    commandList_->SetDescriptorHeaps(
        1,
        descriptorHeaps
    );

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

// =====================================
// 描画後処理
// =====================================
void DirectXCommon::PostDraw() {

    UINT backBufferIndex =
        swapChain_->GetCurrentBackBufferIndex();

    assert(backBufferIndex < kBackBufferCount);

    // RenderTarget → Present
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource =
        swapChainResources_[backBufferIndex].Get();
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(1, &barrier);

    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // コマンドをGPUへ送信
    ID3D12CommandList* commandLists[] = {
        commandList_.Get()
    };

    commandQueue_->ExecuteCommandLists(
        1,
        commandLists
    );

    // 垂直同期
    hr = swapChain_->Present(1, 0);
    assert(SUCCEEDED(hr));

    // GPU同期
    ++fenceValue_;

    hr = commandQueue_->Signal(
        fence_.Get(),
        fenceValue_
    );
    assert(SUCCEEDED(hr));

    if (fence_->GetCompletedValue() < fenceValue_) {

        hr = fence_->SetEventOnCompletion(
            fenceValue_,
            fenceEvent_
        );

        assert(SUCCEEDED(hr));

        WaitForSingleObject(
            fenceEvent_,
            INFINITE
        );
    }

    // 60FPS固定
    UpdateFixFPS();

    // 次フレームの準備
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));

    hr = commandList_->Reset(
        commandAllocator_.Get(),
        nullptr
    );
    assert(SUCCEEDED(hr));
}

// =====================================
// FPS固定の初期化
// =====================================
void DirectXCommon::InitializeFixFPS() {

    reference_ = std::chrono::steady_clock::now();
}

// =====================================
// FPS固定の更新
// =====================================
void DirectXCommon::UpdateFixFPS() {

    const std::chrono::microseconds kMinTime(
        static_cast<int64_t>(1000000.0 / 60.0)
    );

    const std::chrono::microseconds kMinCheckTime(
        static_cast<int64_t>(1000000.0 / 65.0)
    );

    auto now = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            now - reference_
        );

    if (elapsed < kMinCheckTime) {

        while (
            std::chrono::steady_clock::now() - reference_
            < kMinTime
            ) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(1)
            );
        }
    }

    reference_ = std::chrono::steady_clock::now();
}

// =====================================
// 終了処理
// =====================================
void DirectXCommon::Finalize() {

    // ImGuiを先に終了する
    FinalizeImGui();

    // DXC関連の解放
    includeHandler_.Reset();
    dxcCompiler_.Reset();
    dxcUtils_.Reset();

    // Fenceイベントの解放
    if (fenceEvent_ != nullptr) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    // Fence
    fence_.Reset();

    // 深度バッファ
    depthStencilResource_.Reset();

    // バックバッファ
    for (auto& resource : swapChainResources_) {
        resource.Reset();
    }

    // ディスクリプタヒープ
    dsvDescriptorHeap_.Reset();
    srvDescriptorHeap_.Reset();
    rtvDescriptorHeap_.Reset();

    // スワップチェーン
    swapChain_.Reset();

    // コマンド関連
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();

    // デバイス関連
    device_.Reset();
    useAdapter_.Reset();
    dxgiFactory_.Reset();
}

// =====================================
// DXCの初期化
// =====================================
void DirectXCommon::InitializeDXC() {

    // DXCユーティリティの生成
    HRESULT hr = DxcCreateInstance(
        CLSID_DxcUtils,
        IID_PPV_ARGS(dxcUtils_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    // DXCコンパイラの生成
    hr = DxcCreateInstance(
        CLSID_DxcCompiler,
        IID_PPV_ARGS(dxcCompiler_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    // インクルードハンドラの生成
    hr = dxcUtils_->CreateDefaultIncludeHandler(
        includeHandler_.GetAddressOf()
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// ImGuiの初期化
// =====================================
void DirectXCommon::InitializeImGui(HWND hwnd) {

#ifdef USE_IMGUI

    assert(hwnd != nullptr);

    // ImGuiのバージョンチェック
    IMGUI_CHECKVERSION();

    // コンテキスト作成
    ImGui::CreateContext();

    // スタイル設定
    ImGui::StyleColorsDark();

    // Win32の初期化
    ImGui_ImplWin32_Init(hwnd);

    // DirectX12の初期化
    ImGui_ImplDX12_Init(
        device_.Get(),
        kBackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvDescriptorHeap_.Get(),
        srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart()
    );

    // フォントの準備
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();

#else
    (void)hwnd;
#endif
}

// =====================================
// ImGuiの終了処理
// =====================================
void DirectXCommon::FinalizeImGui() {

#ifdef USE_IMGUI

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

#endif
}

// =====================================
// シェーダーコンパイル
// =====================================
Microsoft::WRL::ComPtr<IDxcBlob>
DirectXCommon::CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile
) {
    using Microsoft::WRL::ComPtr;

    assert(dxcUtils_ != nullptr);
    assert(dxcCompiler_ != nullptr);
    assert(includeHandler_ != nullptr);

    // コンパイル開始をログ出力
    std::wstring startMessage = std::format(
        L"Begin CompileShader, path:{}, profile:{}\n",
        filePath,
        profile
    );

    Logger::Log(startMessage);

    // HLSLファイルを読み込む
    ComPtr<IDxcBlobEncoding> shaderSource;

    HRESULT hr = dxcUtils_->LoadFile(
        filePath.c_str(),
        nullptr,
        shaderSource.GetAddressOf()
    );

    assert(SUCCEEDED(hr));

    // コンパイルするソースの情報
    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr =
        shaderSource->GetBufferPointer();

    shaderSourceBuffer.Size =
        shaderSource->GetBufferSize();

    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // コンパイル引数
    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi",
        L"-Qembed_debug",
        L"-Od",
        L"-Zpr",
    };

    // コンパイル実行
    ComPtr<IDxcResult> shaderResult;

    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler_.Get(),
        IID_PPV_ARGS(shaderResult.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    // コンパイル時の警告・エラーを取得
    ComPtr<IDxcBlobUtf8> shaderError;

    hr = shaderResult->GetOutput(
        DXC_OUT_ERRORS,
        IID_PPV_ARGS(shaderError.GetAddressOf()),
        nullptr
    );

    assert(SUCCEEDED(hr));

    if (shaderError != nullptr &&
        shaderError->GetStringLength() != 0) {

        Logger::Log(shaderError->GetStringPointer());

        assert(false);
    }

    // コンパイル成功か確認
    HRESULT compileStatus = S_OK;

    hr = shaderResult->GetStatus(
        &compileStatus
    );

    assert(SUCCEEDED(hr));
    assert(SUCCEEDED(compileStatus));

    // コンパイル済みシェーダーを取得
    ComPtr<IDxcBlob> shaderBlob;

    hr = shaderResult->GetOutput(
        DXC_OUT_OBJECT,
        IID_PPV_ARGS(shaderBlob.GetAddressOf()),
        nullptr
    );

    assert(SUCCEEDED(hr));
    assert(shaderBlob != nullptr);

    // 成功ログ
    std::wstring successMessage = std::format(
        L"Compile Succeeded, path:{}, profile:{}\n",
        filePath,
        profile
    );

    Logger::Log(successMessage);

    // shaderSource、shaderResult、shaderErrorはComPtrにより自動解放される
    return shaderBlob;
}

// =====================================
// バッファリソース生成
// =====================================
Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateBufferResource(size_t sizeInBytes) {

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(resource.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    return resource;
}

// =====================================
// テクスチャファイル読み込み
// =====================================
DirectX::ScratchImage
DirectXCommon::LoadTexture(const std::string& filePath) {

    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);

    HRESULT hr = DirectX::LoadFromWICFile(
        filePathW.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        nullptr,
        image
    );
    assert(SUCCEEDED(hr));

    DirectX::ScratchImage mipImages{};

    hr = DirectX::GenerateMipMaps(
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB,
        0,
        mipImages
    );
    assert(SUCCEEDED(hr));

    return mipImages;
}

// =====================================
// テクスチャリソース生成
// =====================================
Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateTextureResource(
    const DirectX::TexMetadata& metadata) {

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT64>(metadata.width);
    resourceDesc.Height = static_cast<UINT>(metadata.height);
    resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    resourceDesc.DepthOrArraySize =
        static_cast<UINT16>(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension =
        static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(resource.GetAddressOf())
    );

    assert(SUCCEEDED(hr));

    return resource;
}

// =====================================
// テクスチャデータ転送
// =====================================
Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
    const DirectX::ScratchImage& mipImages) {

    assert(texture != nullptr);

    // サブリソース情報
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    HRESULT hr = DirectX::PrepareUpload(
        device_.Get(),
        mipImages.GetImages(),
        mipImages.GetImageCount(),
        mipImages.GetMetadata(),
        subresources
    );
    assert(SUCCEEDED(hr));
    assert(!subresources.empty());

    // 転送用バッファのサイズ
    uint64_t intermediateSize =
        GetRequiredIntermediateSize(
            texture.Get(),
            0,
            static_cast<UINT>(subresources.size())
        );

    // 転送用バッファを生成
    Microsoft::WRL::ComPtr<ID3D12Resource>
        intermediateResource =
        CreateBufferResource(
            static_cast<size_t>(intermediateSize)
        );

    // テクスチャへデータ転送
    UpdateSubresources(
        commandList_.Get(),
        texture.Get(),
        intermediateResource.Get(),
        0,
        0,
        static_cast<UINT>(subresources.size()),
        subresources.data()
    );

    // COPY_DEST → GENERIC_READ
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_GENERIC_READ;

    commandList_->ResourceBarrier(1, &barrier);

    // 転送コマンドがGPUで完了するまで保持が必要
    return intermediateResource;
}