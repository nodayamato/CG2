#pragma once

#include "BlendMode.h"

#include <d3d12.h>
#include <wrl.h>
#include <array>

class DirectXCommon;

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

private:

    // RootSignature生成
    void CreateRootSignature();

    // PSO生成
    void CreateGraphicsPipeline();

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // RootSignature
    Microsoft::WRL::ComPtr<ID3D12RootSignature>
        rootSignature_;

    // ブレンドモードごとのPSO
    std::array<
        Microsoft::WRL::ComPtr<ID3D12PipelineState>,
        kCountOfBlendMode
    > graphicsPipelineStates_;
};