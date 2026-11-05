#include "Model.h"

#include "DirectXCommon.h"
#include "TextureManager.h"

#include <utility>
#include <cassert>
#include <cstring>
#include <limits>

// =====================================
// 初期化
// =====================================
void Model::Initialize(
    DirectXCommon* dxCommon,
    TextureManager* textureManager,
    const std::string& directoryPath,
    const std::string& filename
) {
    assert(dxCommon != nullptr);
    assert(textureManager != nullptr);

    dxCommon_ = dxCommon;
    textureManager_ = textureManager;

    // ---------------------------------
    // モデルファイルを読み込む
    // ---------------------------------
    modelData_ = LoadModelFile(
        directoryPath,
        filename
    );

    assert(!modelData_.vertices.empty());

    assert(
        modelData_.vertices.size() <=
        (std::numeric_limits<uint32_t>::max)()
    );

    vertexCount_ = static_cast<uint32_t>(
        modelData_.vertices.size()
        );

    // ---------------------------------
    // 頂点リソース生成
    // ---------------------------------
    CreateVertexBuffer();

    // ---------------------------------
    // Materialリソース生成
    // ---------------------------------
    CreateMaterialResource();

    // ---------------------------------
    // テクスチャ読み込み
    // ---------------------------------
    textureIndex_ =
        textureManager_->LoadTexture(
            modelData_.material.textureFilePath
        );
}

// =====================================
// 生成済みモデルデータから初期化
// =====================================
void Model::InitializeFromData(
    DirectXCommon* dxCommon,
    TextureManager* textureManager,
    ModelData modelData,
    uint32_t textureIndex
) {
    assert(dxCommon != nullptr);
    assert(textureManager != nullptr);
    assert(!modelData.vertices.empty());

    dxCommon_ = dxCommon;
    textureManager_ = textureManager;

    // 生成済みの頂点データを受け取る
    modelData_ = std::move(modelData);

    // 球などの生成モデルはNode変換なし
    modelData_.rootNode.localMatrix =
        MakeIdentity4x4();

    vertexCount_ = static_cast<uint32_t>(
        modelData_.vertices.size()
        );

    // GPU用頂点バッファ作成
    CreateVertexBuffer();

    // GPU用Material作成
    CreateMaterialResource();

    // 既存のTextureManagerの番号を使用
    textureIndex_ = textureIndex;
}

// =====================================
// 頂点バッファ作成
// =====================================
void Model::CreateVertexBuffer() {

    assert(dxCommon_ != nullptr);
    assert(vertexCount_ > 0);

    // ---------------------------------
    // 頂点リソース生成
    // ---------------------------------
    size_t bufferSize =
        sizeof(VertexData) * vertexCount_;

    vertexResource_ =
        dxCommon_->CreateBufferResource(
            bufferSize
        );

    assert(vertexResource_ != nullptr);

    // ---------------------------------
    // 頂点バッファビュー
    // ---------------------------------
    vertexBufferView_.BufferLocation =
        vertexResource_->GetGPUVirtualAddress();

    vertexBufferView_.SizeInBytes =
        static_cast<UINT>(bufferSize);

    vertexBufferView_.StrideInBytes =
        sizeof(VertexData);

    // ---------------------------------
    // 頂点データを書き込む
    // ---------------------------------
    HRESULT hr = vertexResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertexData_)
    );

    assert(SUCCEEDED(hr));

    std::memcpy(
        vertexData_,
        modelData_.vertices.data(),
        bufferSize
    );
}

// =====================================
// Materialリソース作成
// =====================================
void Model::CreateMaterialResource() {

    assert(dxCommon_ != nullptr);

    // CBV用に256バイト境界で確保
    const size_t bufferSize =
        (sizeof(Material) + 255) &
        ~size_t(255);

    materialResource_ =
        dxCommon_->CreateBufferResource(
            bufferSize
        );

    assert(materialResource_ != nullptr);

    // ---------------------------------
    // Materialを書き込めるようにする
    // ---------------------------------
    HRESULT hr = materialResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&materialData_)
    );

    assert(SUCCEEDED(hr));

    // ---------------------------------
    // 初期値
    // ---------------------------------
    *materialData_ = {};

    materialData_->color = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    // 3Dモデルなのでライティングを有効化
    materialData_->enableLighting = true;

    materialData_->shininess = 32.0f;

    materialData_->uvTransform =
        MakeIdentity4x4();
}

// =====================================
// ライティング設定
// =====================================
void Model::SetEnableLighting(
    bool enableLighting
) {
    assert(materialData_ != nullptr);

    materialData_->enableLighting =
        enableLighting;
}

// =====================================
// 色設定
// =====================================
void Model::SetColor(
    const Vector4& color
) {
    assert(materialData_ != nullptr);

    materialData_->color = color;
}

// =====================================
// 描画
// =====================================
void Model::Draw() {

    assert(dxCommon_ != nullptr);
    assert(textureManager_ != nullptr);
    assert(vertexResource_ != nullptr);
    assert(materialResource_ != nullptr);

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();

    assert(commandList != nullptr);

    // ---------------------------------
    // 頂点バッファ設定
    // ---------------------------------
    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView_
    );

    // ---------------------------------
    // Material (RootParameter[0])
    // ---------------------------------
    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource_->GetGPUVirtualAddress()
    );

    // ---------------------------------
    // Texture (RootParameter[2])
    // ---------------------------------
    commandList->SetGraphicsRootDescriptorTable(
        2,
        textureManager_->GetGPUHandle(
            textureIndex_
        )
    );

    // ---------------------------------
    // モデル描画
    // ---------------------------------
    commandList->DrawInstanced(
        vertexCount_,
        1,
        0,
        0
    );
}

// =====================================
// 終了処理
// =====================================
void Model::Finalize() {

    // ---------------------------------
    // Map解除
    // ---------------------------------
    if (vertexResource_ != nullptr &&
        vertexData_ != nullptr) {

        vertexResource_->Unmap(0, nullptr);
        vertexData_ = nullptr;
    }

    if (materialResource_ != nullptr &&
        materialData_ != nullptr) {

        materialResource_->Unmap(0, nullptr);
        materialData_ = nullptr;
    }

    // ---------------------------------
    // GPUリソース解放
    // ---------------------------------
    vertexResource_.Reset();
    materialResource_.Reset();

    // ---------------------------------
    // CPU側データ初期化
    // ---------------------------------
    modelData_ = {};

    vertexCount_ = 0;
    textureIndex_ = 0;

    dxCommon_ = nullptr;
    textureManager_ = nullptr;
}