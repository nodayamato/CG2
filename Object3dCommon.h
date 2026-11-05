#pragma once

#include "BlendMode.h"

#include <d3d12.h>
#include <wrl.h>
#include <array>

class DirectXCommon;
class Camera;

// =====================================
// 3Dオブジェクト共通処理
// =====================================
class Object3dCommon {
public:

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 3D描画前の共通設定
    void CommonDraw(
        BlendMode blendMode = kBlendModeNormal
    );

    // 終了処理
    void Finalize();

    // DirectXCommon取得
    DirectXCommon* GetDxCommon() const {
        return dxCommon_;
    }

    // 共通Cameraの設定
    void SetDefaultCamera(Camera* camera) {
        defaultCamera_ = camera;
    }

    // 共通Cameraの取得
    Camera* GetDefaultCamera() const {
        return defaultCamera_;
    }

private:

    // RootSignature生成
    void CreateRootSignature();

    // PSO生成
    void CreateGraphicsPipeline();

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // 共通カメラ（所有しない）
    Camera* defaultCamera_ = nullptr;

    // RootSignature
    Microsoft::WRL::ComPtr<ID3D12RootSignature>
        rootSignature_;

    // ブレンドモードごとのPSO
    std::array<
        Microsoft::WRL::ComPtr<ID3D12PipelineState>,
        kCountOfBlendMode
    > graphicsPipelineStates_;
};