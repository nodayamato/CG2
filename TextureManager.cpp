#include "TextureManager.h"

#include "DirectXCommon.h"
#include "SrvManager.h"

#include <cassert>
#include <limits>
#include <utility>

// =====================================
// 初期化
// =====================================
void TextureManager::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager
) {
    assert(dxCommon != nullptr);
    assert(srvManager != nullptr);
    assert(dxCommon->GetDevice() != nullptr);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    textures_.clear();
    textureIndices_.clear();
    textureArrayIndices_.clear();
}

// =====================================
// テクスチャ読み込み
// =====================================
uint32_t TextureManager::LoadTexture(
    const std::string& filePath
) {
    assert(dxCommon_ != nullptr);
    assert(srvManager_ != nullptr);
    assert(!filePath.empty());

    // ---------------------------------
    // 読み込み済みか確認
    // ---------------------------------
    auto it = textureIndices_.find(filePath);

    if (it != textureIndices_.end()) {

        // 既存のSRV番号を返す
        return it->second;
    }

    // ---------------------------------
    // 画像を読み込む
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
        dxCommon_->CreateTextureResource(
            metadata
        );

    assert(textureData.resource != nullptr);

    // ---------------------------------
    // GPUへの転送
    // ---------------------------------
    textureData.uploadResource =
        dxCommon_->UploadTextureData(
            textureData.resource,
            mipImages
        );

    assert(
        textureData.uploadResource != nullptr
    );

    // ---------------------------------
    // SrvManagerから番号を取得
    // ---------------------------------
    assert(srvManager_->CanAllocate());

    uint32_t textureIndex =
        srvManager_->Allocate();

    textureData.srvIndex = textureIndex;

    // ---------------------------------
    // SrvManagerでSRVを生成
    // ---------------------------------
    srvManager_->CreateSRVForTexture2D(
        textureIndex,
        textureData.resource.Get(),
        metadata.format,
        static_cast<uint32_t>(
            metadata.mipLevels
            )
    );

    // ---------------------------------
    // GPUハンドルを取得
    // ---------------------------------
    textureData.gpuHandle =
        srvManager_->GetGPUDescriptorHandle(
            textureIndex
        );

    // ---------------------------------
    // 画像サイズ保存
    // ---------------------------------
    textureData.textureSize = {
        static_cast<float>(metadata.width),
        static_cast<float>(metadata.height)
    };

    // ---------------------------------
    // 管理情報を登録
    // ---------------------------------
    size_t arrayIndex = textures_.size();

    textures_.push_back(
        std::move(textureData)
    );

    textureIndices_.emplace(
        filePath,
        textureIndex
    );

    textureArrayIndices_.emplace(
        textureIndex,
        arrayIndex
    );

    return textureIndex;
}

// =====================================
// SRV番号から配列位置を取得
// =====================================
size_t TextureManager::GetArrayIndex(
    uint32_t textureIndex
) const {

    auto it =
        textureArrayIndices_.find(
            textureIndex
        );

    assert(
        it != textureArrayIndices_.end()
    );

    return it->second;
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
// GPU転送用リソース解放
// =====================================
void TextureManager::ReleaseUploadResources() {

    // GPU転送完了後に呼び出す
    for (TextureData& texture : textures_) {

        texture.uploadResource.Reset();
    }
}

// =====================================
// 終了処理
// =====================================
void TextureManager::Finalize() {

    // テクスチャを解放
    textures_.clear();

    // 管理情報をクリア
    textureIndices_.clear();
    textureArrayIndices_.clear();

    // 外部クラスへの参照解除
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}