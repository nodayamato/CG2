#pragma once

// 前方宣言
class WinApp;
class DirectXCommon;
class SrvManager;

// =====================================
// ImGui管理クラス
// =====================================
class ImGuiManager {
public:

    // 初期化
    void Initialize(
        WinApp* winApp,
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    // フレーム開始
    void Begin();

    // フレーム終了
    void End();

    // 描画
    void Draw();

    // 終了処理
    void Finalize();

private:

    // ウィンドウ管理
    WinApp* winApp_ = nullptr;

    // DirectX12基盤
    DirectXCommon* dxCommon_ = nullptr;

    // SRV管理
    SrvManager* srvManager_ = nullptr;

    // 初期化済みか
    bool isInitialized_ = false;
};