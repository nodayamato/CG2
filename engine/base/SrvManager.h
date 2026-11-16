#pragma once

#include <cstdint>
#include <d3d12.h>

// 前方宣言
class DirectXCommon;

// =====================================
// SRV管理クラス
// =====================================
class SrvManager {
public:

    // SRVの最大数
    static constexpr uint32_t kMaxSRVCount = 128;

    // 既存のSRV配置を保護するための開始番号
    // 0 : ImGui
    // 3 : Particle
    // 5以降 : TextureManager
    static constexpr uint32_t kFirstTextureIndex = 5;

public:

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 使用可能なSRV番号を取得
    uint32_t Allocate();

    // 空きがあるか確認
    bool CanAllocate() const;

    // CPUハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE
        GetCPUDescriptorHandle(uint32_t index) const;

    // GPUハンドル取得
    D3D12_GPU_DESCRIPTOR_HANDLE
        GetGPUDescriptorHandle(uint32_t index) const;

    // テクスチャ用SRV生成
    void CreateSRVForTexture2D(
        uint32_t index,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        uint32_t mipLevels
    );

    // StructuredBuffer用SRV生成
    void CreateSRVForStructuredBuffer(
        uint32_t index,
        ID3D12Resource* resource,
        uint32_t numElements,
        uint32_t structureByteStride
    );

    // 終了処理
    void Finalize();

private:

    // DirectXCommonを参照（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // 次に割り当てるSRV番号
    uint32_t useIndex_ = kFirstTextureIndex;
};