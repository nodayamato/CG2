#pragma once

#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

// =====================================
// スプライト共通処理
// =====================================
class SpriteCommon {
public:

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // スプライト描画前の共通設定
    void CommonDraw();

    // DirectXCommon取得
    DirectXCommon* GetDxCommon() const {
        return dxCommon_;
    }

private:

    // RootSignatureの作成
    void CreateRootSignature();

    // PSOの作成
    void CreateGraphicsPipeline();

private:

    // DirectXCommon
    // 所有しないポインタ
    DirectXCommon* dxCommon_ = nullptr;

    // RootSignature
    Microsoft::WRL::ComPtr<ID3D12RootSignature>
        rootSignature_;

    // PipelineState
    Microsoft::WRL::ComPtr<ID3D12PipelineState>
        graphicsPipelineState_;
};