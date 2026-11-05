#pragma once

#include "Math.h"

// =====================================
// カメラクラス
// =====================================
class Camera {
public:

    // 初期化
    void Initialize();

    // 行列更新
    void Update();

    // ---------------------------------
    // Transform設定
    // ---------------------------------

    // カメラの座標を設定
    void SetTranslate(const Vector3& translate) {
        transform_.translate = translate;
    }

    // カメラの回転を設定
    void SetRotate(const Vector3& rotate) {
        transform_.rotate = rotate;
    }

    // 座標取得
    const Vector3& GetTranslate() const {
        return transform_.translate;
    }

    // 回転取得
    const Vector3& GetRotate() const {
        return transform_.rotate;
    }

    // ---------------------------------
    // カメラ行列取得
    // ---------------------------------

    // DebugCameraのビュー行列を反映
    void SetViewMatrix(const Matrix4x4& viewMatrix);

    // カメラのワールド行列
    const Matrix4x4& GetWorldMatrix() const {
        return worldMatrix_;
    }

    // ビュー行列
    const Matrix4x4& GetViewMatrix() const {
        return viewMatrix_;
    }

    // プロジェクション行列
    const Matrix4x4& GetProjectionMatrix() const {
        return projectionMatrix_;
    }

    // ---------------------------------
    // 射影設定
    // ---------------------------------

    void SetFovY(float fovY) {
        fovY_ = fovY;
    }

    void SetAspectRatio(float aspectRatio) {
        aspectRatio_ = aspectRatio;
    }

    void SetNearClip(float nearClip) {
        nearClip_ = nearClip;
    }

    void SetFarClip(float farClip) {
        farClip_ = farClip;
    }

private:

    // カメラのTransform
    Transform transform_{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -50.0f}
    };

    // カメラワールド行列
    Matrix4x4 worldMatrix_{};

    // ビュー行列
    Matrix4x4 viewMatrix_{};

    // プロジェクション行列
    Matrix4x4 projectionMatrix_{};

    // 射影設定
    float fovY_ = 0.45f;
    float aspectRatio_ = 1280.0f / 720.0f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;
};