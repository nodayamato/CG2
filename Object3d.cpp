#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "Model.h"
#include "Camera.h"

#include <cassert>
#include <cstdint>

// =====================================
// 初期化
// =====================================
void Object3d::Initialize(
    Object3dCommon* object3dCommon
) {
    assert(object3dCommon != nullptr);
    assert(object3dCommon->GetDxCommon() != nullptr);

    // 共通描画クラスを保持
    object3dCommon_ = object3dCommon;

    // Transform初期化
    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };

    // ワールド行列初期化
    worldMatrix_ = MakeIdentity4x4();

    // モデルのローカル行列初期化
    modelLocalMatrix_ = MakeIdentity4x4();

    // WVPリソース生成
    CreateTransformationMatrixResource();
}

// =====================================
// モデル設定
// =====================================
void Object3d::SetModel(Model* model) {

    assert(model != nullptr);

    // モデルへのポインタを保持
    model_ = model;

    // モデルのルートNodeのローカル行列を取得
    modelLocalMatrix_ = model_->GetModelData().rootNode.localMatrix;
}

// =====================================
// WVP用リソース生成
// =====================================
void Object3d::CreateTransformationMatrixResource() {

    DirectXCommon* dxCommon =
        object3dCommon_->GetDxCommon();

    assert(dxCommon != nullptr);

    // ConstantBufferは256バイト単位で確保
    const size_t bufferSize =
        (sizeof(TransformationMatrix) + 255) & ~size_t(255);

    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(bufferSize);

    assert(transformationMatrixResource_ != nullptr);

    // CPUから書き込めるようにMap
    HRESULT hr =
        transformationMatrixResource_->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(
                &transformationMatrixData_
                )
        );

    assert(SUCCEEDED(hr));

    // 初期値
    transformationMatrixData_->WVP =
        MakeIdentity4x4();

    transformationMatrixData_->World =
        MakeIdentity4x4();

    transformationMatrixData_->WorldInverseTranspose =
        MakeIdentity4x4();
}

// =====================================
// 共通Cameraを使用して更新
// =====================================
void Object3d::Update() {

    assert(object3dCommon_ != nullptr);

    // 共通Cameraを取得
    Camera* camera =
        object3dCommon_->GetDefaultCamera();

    assert(camera != nullptr);

    // CameraからView・Projectionを取得し、
    // 既存の行列更新処理を呼び出す
    Update(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );
}

// =====================================
// 更新
// =====================================
void Object3d::Update(
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix
) {
    assert(transformationMatrixData_ != nullptr);

    // ---------------------------------
    // Transformからワールド行列を作成
    // ---------------------------------

    Matrix4x4 objectWorldMatrix =
        MakeAffineMatrix(
            transform_.scale,
            transform_.rotate,
            transform_.translate
        );

    // ---------------------------------
    // モデルのローカル行列を反映
    // ---------------------------------

    worldMatrix_ =
        Multiply(
            modelLocalMatrix_,
            objectWorldMatrix
        );

    // ---------------------------------
    // ViewProjection行列
    // ---------------------------------

    Matrix4x4 viewProjectionMatrix =
        Multiply(
            viewMatrix,
            projectionMatrix
        );

    // ---------------------------------
    // WVP行列
    // ---------------------------------

    Matrix4x4 worldViewProjectionMatrix =
        Multiply(
            worldMatrix_,
            viewProjectionMatrix
        );

    // ---------------------------------
    // ワールド逆転置行列
    // ---------------------------------

    Matrix4x4 worldInverseMatrix =
        Inverse(worldMatrix_);

    Matrix4x4 worldInverseTranspose =
        MakeIdentity4x4();

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {

            worldInverseTranspose.m[y][x] =
                worldInverseMatrix.m[x][y];
        }
    }

    // ---------------------------------
    // GPUへ転送する行列を更新
    // ---------------------------------

    transformationMatrixData_->WVP =
        worldViewProjectionMatrix;

    transformationMatrixData_->World =
        worldMatrix_;

    transformationMatrixData_->WorldInverseTranspose =
        worldInverseTranspose;
}

// =====================================
// 描画用のWVPを設定
// =====================================
void Object3d::BindTransformationMatrix() {

    assert(object3dCommon_ != nullptr);
    assert(transformationMatrixResource_ != nullptr);

    DirectXCommon* dxCommon =
        object3dCommon_->GetDxCommon();

    ID3D12GraphicsCommandList* commandList =
        dxCommon->GetCommandList();

    assert(commandList != nullptr);

    // RootParameter[1] にWVPを設定
    // Object3dCommonのRootSignatureに対応
    commandList->SetGraphicsRootConstantBufferView(
        1,
        transformationMatrixResource_->GetGPUVirtualAddress()
    );
}

// =====================================
// 描画
// =====================================
void Object3d::Draw() {

    assert(object3dCommon_ != nullptr);
    assert(model_ != nullptr);

    // オブジェクトごとのWVPを設定
    BindTransformationMatrix();

    // 登録されているモデルを描画
    model_->Draw();
}

// =====================================
// 終了処理
// =====================================
void Object3d::Finalize() {

    // GPUへの書き込みを終了
    if (transformationMatrixResource_ != nullptr &&
        transformationMatrixData_ != nullptr) {

        transformationMatrixResource_->Unmap(
            0,
            nullptr
        );

        transformationMatrixData_ = nullptr;
    }

    // WVPリソース解放
    transformationMatrixResource_.Reset();

    // 所有していないポインタを解除
    model_ = nullptr;
    object3dCommon_ = nullptr;
}