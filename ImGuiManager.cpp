#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

#include <cassert>

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

#endif

// =====================================
// 初期化
// =====================================
void ImGuiManager::Initialize(
    WinApp* winApp,
    DirectXCommon* dxCommon,
    SrvManager* srvManager
) {

#ifdef USE_IMGUI

    assert(!isInitialized_);
    assert(winApp != nullptr);
    assert(dxCommon != nullptr);
    assert(srvManager != nullptr);

    assert(winApp->GetHwnd() != nullptr);
    assert(dxCommon->GetDevice() != nullptr);
    assert(dxCommon->GetCommandList() != nullptr);

    // 外部クラスを保持
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // ---------------------------------
    // ImGuiのコンテキストを作成
    // ---------------------------------
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    // ---------------------------------
    // スタイル設定
    // ---------------------------------
    ImGui::StyleColorsDark();

    // ---------------------------------
    // Win32の初期化
    // ---------------------------------
    bool win32Result =
        ImGui_ImplWin32_Init(
            winApp_->GetHwnd()
        );

    assert(win32Result);

    // ---------------------------------
    // DirectX12の初期化
    // ---------------------------------

    // DirectXCommonと共通のSRVヒープ
    ID3D12DescriptorHeap* srvHeap =
        dxCommon_->GetSRVDescriptorHeap();

    assert(srvHeap != nullptr);

    // ImGuiは0番のSRVを使用する
    constexpr uint32_t kImGuiSrvIndex = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        srvManager_->GetCPUDescriptorHandle(
            kImGuiSrvIndex
        );

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        srvManager_->GetGPUDescriptorHandle(
            kImGuiSrvIndex
        );

    bool dx12Result =
        ImGui_ImplDX12_Init(
            dxCommon_->GetDevice(),
            DirectXCommon::kBackBufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            srvHeap,
            cpuHandle,
            gpuHandle
        );

    assert(dx12Result);

    // ---------------------------------
    // フォントの準備
    // ---------------------------------
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();

    isInitialized_ = true;

#else

    (void)winApp;
    (void)dxCommon;
    (void)srvManager;

#endif
}

// =====================================
// フレーム開始
// =====================================
void ImGuiManager::Begin() {

#ifdef USE_IMGUI

    assert(isInitialized_);

    // DirectX12の新しいフレーム
    ImGui_ImplDX12_NewFrame();

    // Win32の新しいフレーム
    ImGui_ImplWin32_NewFrame();

    // ImGuiのフレーム開始
    ImGui::NewFrame();

#endif
}

// =====================================
// フレーム終了
// =====================================
void ImGuiManager::End() {

#ifdef USE_IMGUI

    assert(isInitialized_);

    // ImGuiの描画データを確定
    ImGui::Render();

#endif
}

// =====================================
// 描画
// =====================================
void ImGuiManager::Draw() {

#ifdef USE_IMGUI

    assert(isInitialized_);
    assert(dxCommon_ != nullptr);

    // 描画データ取得
    ImDrawData* drawData =
        ImGui::GetDrawData();

    if (drawData == nullptr) {
        return;
    }

    // 現在のコマンドリストに描画
    ImGui_ImplDX12_RenderDrawData(
        drawData,
        dxCommon_->GetCommandList()
    );

#endif
}

// =====================================
// 終了処理
// =====================================
void ImGuiManager::Finalize() {

#ifdef USE_IMGUI

    if (isInitialized_) {

        // DirectX12の終了
        ImGui_ImplDX12_Shutdown();

        // Win32の終了
        ImGui_ImplWin32_Shutdown();

        // ImGuiコンテキスト破棄
        ImGui::DestroyContext();

        isInitialized_ = false;
    }

#endif

    // 外部クラスへの参照を解除
    winApp_ = nullptr;
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}