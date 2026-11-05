#pragma once

#include "ModelData.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>

class DirectXCommon;
class TextureManager;

// =====================================
// 3Dモデルクラス
// =====================================
class Model {
public:

    // 初期化
    void Initialize(
        DirectXCommon* dxCommon,
        TextureManager* textureManager,
        const std::string& directoryPath,
        const std::string& filename
    );

    // 生成済みモデルデータから初期化
    void InitializeFromData(
        DirectXCommon* dxCommon,
        TextureManager* textureManager,
        ModelData modelData,
        uint32_t textureIndex
    );

    // テクスチャ切り替え
    void SetTextureIndex(uint32_t textureIndex) {
        textureIndex_ = textureIndex;
    }

    // GPU用Materialデータ取得
    Material* GetMaterialData() const {
        return materialData_;
    }

    // 描画
    // Object3dCommon::CommonDraw() の後に呼ぶ
    void Draw();

    // モデルデータ取得
    const ModelData& GetModelData() const {
        return modelData_;
    }

    // テクスチャ番号取得
    uint32_t GetTextureIndex() const {
        return textureIndex_;
    }

    // 頂点数取得
    uint32_t GetVertexCount() const {
        return vertexCount_;
    }

    // 明るさ計算の有効・無効
    void SetEnableLighting(bool enableLighting);

    // 色設定
    void SetColor(const Vector4& color);

    // リソース解放
    void Finalize();

private:

    // 頂点リソース作成
    void CreateVertexBuffer();

    // Materialリソース作成
    void CreateMaterialResource();

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // TextureManager（所有しない）
    TextureManager* textureManager_ = nullptr;

    // モデルファイルから読み込んだデータ
    ModelData modelData_{};

    // 頂点数
    uint32_t vertexCount_ = 0;

    // TextureManagerで管理する番号
    uint32_t textureIndex_ = 0;

    // 頂点リソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        vertexResource_;

    // Materialリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        materialResource_;

    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // Mapしたデータ
    VertexData* vertexData_ = nullptr;
    Material* materialData_ = nullptr;
};