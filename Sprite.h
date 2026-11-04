#pragma once

#include "Math.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class SpriteCommon;

// =====================================
// スプライトクラス
// =====================================
class Sprite {
public:

    // 初期化
    void Initialize(
        SpriteCommon* spriteCommon,
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
        const Vector2& textureSize
    );

    // 更新
    void Update();

    // 描画
    // SpriteCommon::CommonDraw() の後に呼ぶ
    void Draw();

    // 座標設定
    void SetPosition(const Vector2& position) {
        position_ = position;
    }

    // 座標取得
    const Vector2& GetPosition() const {
        return position_;
    }

    // サイズ設定
    void SetSize(const Vector2& size) {
        size_ = size;
    }

    // サイズ取得
    const Vector2& GetSize() const {
        return size_;
    }

    // 回転角度設定（ラジアン）
    void SetRotation(float rotation) {
        rotation_ = rotation;
    }

    // 回転角度取得
    float GetRotation() const {
        return rotation_;
    }

    // 色設定
    void SetColor(const Vector4& color) {
        color_ = color;
    }

    // 表示するテクスチャ範囲の左上座標
    void SetTextureLeftTop(const Vector2& leftTop) {
        textureLeftTop_ = leftTop;
    }

    // 表示するテクスチャ範囲のサイズ
    void SetTextureSize(const Vector2& size) {
        texturePartSize_ = size;
    }

    // テクスチャのGPUハンドル設定
    void SetTextureHandle(
        D3D12_GPU_DESCRIPTOR_HANDLE handle
    ) {
        textureHandle_ = handle;
    }

private:

    // 頂点・インデックスバッファ作成
    void CreateVertexBuffer();
    void CreateIndexBuffer();

    // Material・WVPリソース作成
    void CreateMaterialResource();
    void CreateTransformationMatrixResource();

    // 頂点情報の更新
    void UpdateVertexData();

private:

    // 共通描画クラス（所有しない）
    SpriteCommon* spriteCommon_ = nullptr;

    // テクスチャSRV
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    // 座標
    Vector2 position_{ 0.0f, 0.0f };

    // 描画サイズ
    Vector2 size_{ 100.0f, 100.0f };

    // テクスチャ全体のサイズ
    Vector2 textureSize_{ 1.0f, 1.0f };

    // テクスチャ切り取り開始位置（ピクセル）
    Vector2 textureLeftTop_{ 0.0f, 0.0f };

    // テクスチャ切り取りサイズ（ピクセル）
    Vector2 texturePartSize_{ 1.0f, 1.0f };

    // Z軸回転（ラジアン）
    float rotation_ = 0.0f;

    // 描画色
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };

    // 頂点リソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        vertexResource_;

    // インデックスリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        indexResource_;

    // Materialリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        materialResource_;

    // WVPリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        transformationMatrixResource_;

    // GPU転送用ビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    // Mapした書き込み先
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;
};