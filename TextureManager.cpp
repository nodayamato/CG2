#include "TextureManager.h"
#include "DirectXCommon.h"

#include <cassert>
#include <utility>

// =====================================
// 初期化
// =====================================
void TextureManager::Initialize(
    DirectXCommon* dxCommon
) {
    assert(dxCommon != nullptr);
    assert(dxCommon->GetDevice() != nullptr);

    dxCommon_ = dxCommon;

    textures_.clear();
    textureIndices_.clear();
}

// =====================================
// テクスチャ読み込み
// =====================================
uint32_t TextureManager::LoadTexture(
    const std::string& filePath
) {
    assert(dxCommon_ != nullptr);
    assert(!filePath.empty());

    // ---------------------------------
    // 読み込み済みか確認
    // ---------------------------------
    auto it = textureIndices_.find(filePath);

    if (it != textureIndices_.end()) {

        // 読み込み済みなら番号を返す
        return it->second;
    }

    // ---------------------------------
    // 空きSRVインデックスを取得
    // ---------------------------------
    uint32_t textureIndex =
        kFirstTextureIndex +
        static_cast<uint32_t>(textures_.size());

    // SRVヒープの範囲を確認
    assert(textureIndex < kMaxSRVCount);

    // ---------------------------------
    // ファイルからテクスチャ読み込み
    // ---------------------------------
    DirectX::ScratchImage mipImages =
        DirectXCommon::LoadTexture(filePath);

    const DirectX::TexMetadata& metadata =
        mipImages.GetMetadata();

    // ---------------------------------
    // テクスチャリソース生成
    // ---------------------------------
    TextureData textureData{};

    textureData.filePath = filePath;

    textureData.resource =
        dxCommon_->CreateTextureResource(metadata);

    assert(textureData.resource != nullptr);

    // ---------------------------------
    // GPUへの転送を記録
    // ---------------------------------
    textureData.uploadResource =
        dxCommon_->UploadTextureData(
            textureData.resource,
            mipImages
        );

    assert(textureData.uploadResource != nullptr);

    // ---------------------------------
    // SRVの設定
    // ---------------------------------
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    srvDesc.Format = metadata.format;

    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Texture2D.MipLevels =
        static_cast<UINT>(metadata.mipLevels);

    // ---------------------------------
    // CPU・GPUハンドル取得
    // ---------------------------------
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        dxCommon_->GetSRVCPUDescriptorHandle(
            textureIndex
        );

    textureData.gpuHandle =
        dxCommon_->GetSRVGPUDescriptorHandle(
            textureIndex
        );

    // ---------------------------------
    // SRV生成
    // ---------------------------------
    ID3D12Device* device =
        dxCommon_->GetDevice();

    assert(device != nullptr);

    device->CreateShaderResourceView(
        textureData.resource.Get(),
        &srvDesc,
        cpuHandle
    );

    // ---------------------------------
    // テクスチャサイズ保存
    // ---------------------------------
    textureData.textureSize = {
        static_cast<float>(metadata.width),
        static_cast<float>(metadata.height)
    };

    // ---------------------------------
    // 管理リストに登録
    // ---------------------------------
    textures_.push_back(
        std::move(textureData)
    );

    textureIndices_.emplace(
        filePath,
        textureIndex
    );

    // 登録した番号を返す
    return textureIndex;
}

// =====================================
// テクスチャ配列のインデックス取得
// =====================================
size_t TextureManager::GetArrayIndex(
    uint32_t textureIndex
) const {

    assert(textureIndex >= kFirstTextureIndex);

    size_t arrayIndex =
        static_cast<size_t>(
            textureIndex - kFirstTextureIndex
            );

    assert(arrayIndex < textures_.size());

    return arrayIndex;
}

// =====================================
// GPUハンドル取得
// =====================================
D3D12_GPU_DESCRIPTOR_HANDLE
TextureManager::GetGPUHandle(
    uint32_t textureIndex
) const {

    size_t arrayIndex =
        GetArrayIndex(textureIndex);

    return textures_[arrayIndex].gpuHandle;
}

// =====================================
// テクスチャサイズ取得
// =====================================
Vector2 TextureManager::GetTextureSize(
    uint32_t textureIndex
) const {

    size_t arrayIndex =
        GetArrayIndex(textureIndex);

    return textures_[arrayIndex].textureSize;
}

// =====================================
// GPU転送用リソースの解放
// =====================================
void TextureManager::ReleaseUploadResources() {

    // 注意：
    // 転送コマンドのGPU実行完了後に呼ぶこと

    for (TextureData& texture : textures_) {

        texture.uploadResource.Reset();
    }
}

// =====================================
// 終了処理
// =====================================
void TextureManager::Finalize() {

    // ComPtrのリソースを解放
    textures_.clear();

    // テクスチャ番号の登録情報を消去
    textureIndices_.clear();

    dxCommon_ = nullptr;
}