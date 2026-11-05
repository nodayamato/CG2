#include "Camera.h"

#include "WinApp.h"

#include <cassert>

// =====================================
// 初期化
// =====================================
void Camera::Initialize() {

    // カメラの拡大率
    transform_.scale = {
        1.0f, 1.0f, 1.0f
    };

    // カメラの回転
    transform_.rotate = {
        0.0f, 0.0f, 0.0f
    };

    // カメラの初期座標
    transform_.translate = {
        0.0f, 0.0f, -50.0f
    };

    // 画角
    fovY_ = 0.45f;

    // アスペクト比
    aspectRatio_ =
        static_cast<float>(WinApp::kClientWidth) /
        static_cast<float>(WinApp::kClientHeight);

    // Near / Far
    nearClip_ = 0.1f;
    farClip_ = 1000.0f;

    // 初期行列を計算
    Update();
}

// =====================================
// カメラ行列更新
// =====================================
void Camera::Update() {

    assert(aspectRatio_ > 0.0f);
    assert(fovY_ > 0.0f);
    assert(nearClip_ > 0.0f);
    assert(farClip_ > nearClip_);

    // ---------------------------------
    // カメラのワールド行列
    // ---------------------------------

    worldMatrix_ = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate
    );

    // ---------------------------------
    // ビュー行列
    // カメラワールド行列の逆行列
    // ---------------------------------

    viewMatrix_ = Inverse(
        worldMatrix_
    );

    // ---------------------------------
    // プロジェクション行列
    // ---------------------------------

    projectionMatrix_ = MakePerspectiveFovMatrix(
        fovY_,
        aspectRatio_,
        nearClip_,
        farClip_
    );
}

// =====================================
// 外部からビュー行列を設定
// DebugCameraとの連携に使用
// =====================================
void Camera::SetViewMatrix(
    const Matrix4x4& viewMatrix
) {
    // View行列
    viewMatrix_ = viewMatrix;

    // CameraのWorld行列
    worldMatrix_ = Inverse(viewMatrix_);

    // ワールド座標も更新
    transform_.translate = {
        worldMatrix_.m[3][0],
        worldMatrix_.m[3][1],
        worldMatrix_.m[3][2]
    };
}