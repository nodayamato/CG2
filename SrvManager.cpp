#include "SrvManager.h"

#include "DirectXCommon.h"

#include <cassert>

// =====================================
// 初期化
// =====================================
void SrvManager::Initialize(
    DirectXCommon* dxCommon
) {
    assert(dxCommon != nullptr);
    assert(dxCommon->GetDevice() != nullptr);
    assert(dxCommon->GetSRVDescriptorHeap() != nullptr);

    // SrvManagerとDirectXCommonの容量を合わせる
    static_assert(
        kMaxSRVCount == DirectXCommon::kMaxSRVCount
        );

    dxCommon_ = dxCommon;

    // 0～4番は現在の既存実装用に予約
    useIndex_ = kFirstTextureIndex;
}

// =====================================
// SRVを割り当てられるか確認
// =====================================
bool SrvManager::CanAllocate() const {

    if (dxCommon_ == nullptr) {
        return false;
    }

    return useIndex_ < kMaxSRVCount;
}

// =====================================
// SRV番号を割り当てる
// =====================================
uint32_t SrvManager::Allocate() {

    assert(CanAllocate());

    uint32_t index = useIndex_;

    ++useIndex_;

    return index;
}

// =====================================
// CPUディスクリプタハンドル取得
// =====================================
D3D12_CPU_DESCRIPTOR_HANDLE
SrvManager::GetCPUDescriptorHandle(
    uint32_t index
) const {
    assert(dxCommon_ != nullptr);
    assert(index < kMaxSRVCount);

    return dxCommon_->GetSRVCPUDescriptorHandle(
        index
    );
}

// =====================================
// GPUディスクリプタハンドル取得
// =====================================
D3D12_GPU_DESCRIPTOR_HANDLE
SrvManager::GetGPUDescriptorHandle(
    uint32_t index
) const {
    assert(dxCommon_ != nullptr);
    assert(index < kMaxSRVCount);

    return dxCommon_->GetSRVGPUDescriptorHandle(
        index
    );
}

// =====================================
// Texture2D用SRV生成
// =====================================
void SrvManager::CreateSRVForTexture2D(
    uint32_t index,
    ID3D12Resource* resource,
    DXGI_FORMAT format,
    uint32_t mipLevels
) {
    assert(dxCommon_ != nullptr);
    assert(resource != nullptr);
    assert(index < kMaxSRVCount);
    assert(mipLevels > 0);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    srvDesc.Format = format;

    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Texture2D.MipLevels = mipLevels;

    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    // SRV作成
    dxCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        GetCPUDescriptorHandle(index)
    );
}

// =====================================
// StructuredBuffer用SRV生成
// =====================================
void SrvManager::CreateSRVForStructuredBuffer(
    uint32_t index,
    ID3D12Resource* resource,
    uint32_t numElements,
    uint32_t structureByteStride
) {
    assert(dxCommon_ != nullptr);
    assert(resource != nullptr);
    assert(index < kMaxSRVCount);
    assert(numElements > 0);
    assert(structureByteStride > 0);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    srvDesc.Format = DXGI_FORMAT_UNKNOWN;

    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_BUFFER;

    srvDesc.Buffer.FirstElement = 0;

    srvDesc.Buffer.NumElements = numElements;

    srvDesc.Buffer.StructureByteStride =
        structureByteStride;

    srvDesc.Buffer.Flags =
        D3D12_BUFFER_SRV_FLAG_NONE;

    // SRV作成
    dxCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        GetCPUDescriptorHandle(index)
    );
}

// =====================================
// 終了処理
// =====================================
void SrvManager::Finalize() {

    // SRVヒープは現在DirectXCommonが所有する
    // ため、ここでは解放しない

    dxCommon_ = nullptr;

    useIndex_ = kFirstTextureIndex;
}