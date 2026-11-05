#pragma once

#include "Math.h"

#include <d3d12.h>
#include <wrl.h>

class Object3dCommon;
class Model;

// =====================================
// 3Dオブジェクトクラス
// =====================================
class Object3d {
public:

    // 初期化
    void Initialize(Object3dCommon* object3dCommon);

    // 更新
    void Update(
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projectionMatrix
    );

    // WVPのConstantBufferを描画用に設定
    void BindTransformationMatrix();

    // 描画
    // Object3dCommon::CommonDraw() の後に呼ぶ
    void Draw();

    // 終了処理
    void Finalize();

    // ---------------------------------
    // Transform
    // ---------------------------------

    // 座標設定
    void SetPosition(const Vector3& position) {
        transform_.translate = position;
    }

    // 座標取得
    Vector3 GetPosition() const {
        return transform_.translate;
    }

    // 回転設定
    void SetRotation(const Vector3& rotation) {
        transform_.rotate = rotation;
    }

    // 回転取得
    Vector3 GetRotation() const {
        return transform_.rotate;
    }

    // 拡大率設定
    void SetScale(const Vector3& scale) {
        transform_.scale = scale;
    }

    // 拡大率取得
    Vector3 GetScale() const {
        return transform_.scale;
    }

    // Transformをまとめて設定
    void SetTransform(const Transform& transform) {
        transform_ = transform;
    }

    // Transform取得
    const Transform& GetTransform() const {
        return transform_;
    }

    // ---------------------------------
    // 行列
    // ---------------------------------

    // ワールド行列取得
    const Matrix4x4& GetWorldMatrix() const {
        return worldMatrix_;
    }

    // モデルのローカル行列を設定
    void SetModelLocalMatrix(
        const Matrix4x4& localMatrix
    ) {
        modelLocalMatrix_ = localMatrix;
    }

    // ---------------------------------
    // モデル
    // ---------------------------------

    // モデル設定
    void SetModel(Model* model);

    // モデル取得
    Model* GetModel() const {
        return model_;
    }

private:

    // WVP用リソース作成
    void CreateTransformationMatrixResource();

private:

    // 共通描画処理（所有しない）
    Object3dCommon* object3dCommon_ = nullptr;

    // モデル（所有しない）
    Model* model_ = nullptr;

    // オブジェクトごとのTransform
    Transform transform_{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    // オブジェクトごとのワールド行列
    Matrix4x4 worldMatrix_{};

    // モデルのローカル行列
    Matrix4x4 modelLocalMatrix_{};

    // WVP用GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource>
        transformationMatrixResource_;

    // GPUに書き込むデータ
    TransformationMatrix* transformationMatrixData_ = nullptr;
};