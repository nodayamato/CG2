#include "Sprite.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "WinApp.h"

#include <cassert>
#include <cmath>

// =====================================
// 初期化
// =====================================
void Sprite::Initialize(
    SpriteCommon* spriteCommon,
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    const Vector2& textureSize
) {
    assert(spriteCommon != nullptr);
    assert(spriteCommon->GetDxCommon() != nullptr);
    assert(textureSize.x > 0.0f);
    assert(textureSize.y > 0.0f);

    spriteCommon_ = spriteCommon;

    textureHandle_ = textureHandle;
    textureSize_ = textureSize;

    // 初期状態はテクスチャ全体を表示
    size_ = textureSize;
    textureLeftTop_ = { 0.0f, 0.0f };
    texturePartSize_ = textureSize;

    // リソース生成
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateMaterialResource();
    CreateTransformationMatrixResource();

    // 初期データを更新
    Update();
}

// =====================================
// 頂点バッファ作成
// =====================================
void Sprite::CreateVertexBuffer() {

    DirectXCommon* dxCommon =
        spriteCommon_->GetDxCommon();

    // 4頂点分のバッファ
    vertexResource_ =
        dxCommon->CreateBufferResource(
            sizeof(VertexData) * 4
        );

    assert(vertexResource_ != nullptr);

    // 頂点バッファビュー
    vertexBufferView_.BufferLocation =
        vertexResource_->GetGPUVirtualAddress();

    vertexBufferView_.SizeInBytes =
        sizeof(VertexData) * 4;

    vertexBufferView_.StrideInBytes =
        sizeof(VertexData);

    // 書き込み先
    HRESULT hr = vertexResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertexData_)
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// インデックスバッファ作成
// =====================================
void Sprite::CreateIndexBuffer() {

    DirectXCommon* dxCommon =
        spriteCommon_->GetDxCommon();

    // 6インデックス分のバッファ
    indexResource_ =
        dxCommon->CreateBufferResource(
            sizeof(uint32_t) * 6
        );

    assert(indexResource_ != nullptr);

    // インデックスバッファビュー
    indexBufferView_.BufferLocation =
        indexResource_->GetGPUVirtualAddress();

    indexBufferView_.SizeInBytes =
        sizeof(uint32_t) * 6;

    indexBufferView_.Format =
        DXGI_FORMAT_R32_UINT;

    // 書き込み先
    HRESULT hr = indexResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&indexData_)
    );

    assert(SUCCEEDED(hr));

    // 三角形1枚目
    indexData_[0] = 0;
    indexData_[1] = 1;
    indexData_[2] = 2;

    // 三角形2枚目
    indexData_[3] = 1;
    indexData_[4] = 3;
    indexData_[5] = 2;
}

// =====================================
// Materialリソース作成
// =====================================
void Sprite::CreateMaterialResource() {

    DirectXCommon* dxCommon =
        spriteCommon_->GetDxCommon();

    // CBVは256バイト境界を確保
    materialResource_ =
        dxCommon->CreateBufferResource(256);

    assert(materialResource_ != nullptr);

    HRESULT hr = materialResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&materialData_)
    );

    assert(SUCCEEDED(hr));

    // 初期値
    *materialData_ = {};

    materialData_->color =
    { 1.0f, 1.0f, 1.0f, 1.0f };

    materialData_->enableLighting = false;
    materialData_->shininess = 32.0f;

    materialData_->uvTransform =
        MakeIdentity4x4();
}

// =====================================
// WVPリソース作成
// =====================================
void Sprite::CreateTransformationMatrixResource() {

    DirectXCommon* dxCommon =
        spriteCommon_->GetDxCommon();

    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(256);

    assert(transformationMatrixResource_ != nullptr);

    HRESULT hr =
        transformationMatrixResource_->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(
                &transformationMatrixData_
                )
        );

    assert(SUCCEEDED(hr));

    transformationMatrixData_->WVP =
        MakeIdentity4x4();

    transformationMatrixData_->World =
        MakeIdentity4x4();

    transformationMatrixData_->WorldInverseTranspose =
        MakeIdentity4x4();
}

// =====================================
// 頂点データ更新
// =====================================
void Sprite::UpdateVertexData() {

    assert(vertexData_ != nullptr);

    // テクスチャサイズで割りUVへ変換
    const float left = textureLeftTop_.x / textureSize_.x;
    const float top = textureLeftTop_.y / textureSize_.y;
    const float right = (textureLeftTop_.x + texturePartSize_.x) / textureSize_.x;
    const float bottom = (textureLeftTop_.y + texturePartSize_.y) / textureSize_.y;

    // 頂点のローカル座標
    const float width = size_.x;
    const float height = size_.y;

    // 左下
    vertexData_[0].position = { 0.0f, height, 0.0f, 1.0f };
    vertexData_[0].texcoord = { left, bottom };
    vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

    // 左上
    vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { left, top };
    vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

    // 右下
    vertexData_[2].position = { width, height, 0.0f, 1.0f };
    vertexData_[2].texcoord = { right, bottom };
    vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

    // 右上
    vertexData_[3].position = { width, 0.0f, 0.0f, 1.0f };
    vertexData_[3].texcoord = { right, top };
    vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };
}

// =====================================
// 更新
// =====================================
void Sprite::Update() {

    assert(spriteCommon_ != nullptr);

    // ---------------------------------
    // 頂点更新
    // ---------------------------------
    UpdateVertexData();

    // ---------------------------------
    // ワールド行列
    // ---------------------------------
    Transform transform{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, rotation_},
        {position_.x, position_.y, 0.0f}
    };

    Matrix4x4 worldMatrix =
        MakeAffineMatrix(
            transform.scale,
            transform.rotate,
            transform.translate
        );

    // ---------------------------------
    // 2D用の正射影
    // ---------------------------------
    Matrix4x4 viewMatrix =
        MakeIdentity4x4();

    Matrix4x4 projectionMatrix =
        MakeOrthographicMatrix(
            0.0f,
            0.0f,
            static_cast<float>(WinApp::kClientWidth),
            static_cast<float>(WinApp::kClientHeight),
            0.0f,
            100.0f
        );

    Matrix4x4 wvpMatrix =
        Multiply(
            worldMatrix,
            Multiply(viewMatrix, projectionMatrix)
        );

    // GPUへ書き込み
    transformationMatrixData_->WVP = wvpMatrix;

    transformationMatrixData_->World =
        worldMatrix;

    transformationMatrixData_->WorldInverseTranspose =
        MakeIdentity4x4();

    // 色を更新
    materialData_->color = color_;
}

// =====================================
// 描画
// =====================================
void Sprite::Draw() {

    assert(spriteCommon_ != nullptr);

    DirectXCommon* dxCommon =
        spriteCommon_->GetDxCommon();

    ID3D12GraphicsCommandList* commandList =
        dxCommon->GetCommandList();

    assert(commandList != nullptr);

    // 頂点バッファ
    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView_
    );

    // インデックスバッファ
    commandList->IASetIndexBuffer(
        &indexBufferView_
    );

    // Material (b0, PixelShader)
    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource_->GetGPUVirtualAddress()
    );

    // WVP (b0, VertexShader)
    commandList->SetGraphicsRootConstantBufferView(
        1,
        transformationMatrixResource_->GetGPUVirtualAddress()
    );

    // テクスチャSRV (t0, PixelShader)
    commandList->SetGraphicsRootDescriptorTable(
        2,
        textureHandle_
    );

    // 四角形の描画
    commandList->DrawIndexedInstanced(
        6,
        1,
        0,
        0,
        0
    );
}