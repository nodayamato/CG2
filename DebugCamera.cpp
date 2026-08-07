#include "DebugCamera.h"

/// <summary>
/// Vector3を行列で変換する
/// 平行移動は使用しない
/// </summary>
static Vector3 TransformNormal(
	const Vector3& vector,
	const Matrix4x4& matrix)
{
	Vector3 result{};

	result.x =
		vector.x * matrix.m[0][0] +
		vector.y * matrix.m[1][0] +
		vector.z * matrix.m[2][0];

	result.y =
		vector.x * matrix.m[0][1] +
		vector.y * matrix.m[1][1] +
		vector.z * matrix.m[2][1];

	result.z =
		vector.x * matrix.m[0][2] +
		vector.y * matrix.m[1][2] +
		vector.z * matrix.m[2][2];

	return result;
}

/// <summary>
/// 初期化
/// </summary>
void DebugCamera::Initialize()
{
	// 回転行列を単位行列で初期化
	matRot_ = MakeIdentity4x4();

	// 注視点
	target_ = {
		0.0f,
		0.0f,
		0.0f
	};

	// 注視点からカメラまでの距離
	distance_ = 50.0f;

	// ------------------------------
	// 初期カメラ位置を計算
	// ------------------------------

	Vector3 offset = {
		0.0f,
		0.0f,
		-distance_
	};

	offset =
		TransformNormal(
			offset,
			matRot_);

	Vector3 cameraPosition = {
		target_.x + offset.x,
		target_.y + offset.y,
		target_.z + offset.z
	};

	// ------------------------------
	// カメラのWorld行列
	// ------------------------------

	Matrix4x4 translateMatrix =
		MakeTranslateMatrix(cameraPosition);

	Matrix4x4 cameraMatrix =
		Multiply(
			matRot_,
			translateMatrix);

	// Worldの逆行列がView
	viewMatrix_ =
		Inverse(cameraMatrix);

	// ------------------------------
	// 射影行列
	// ------------------------------

	projectionMatrix_ =
		MakePerspectiveFovMatrix(
			0.45f,
			1280.0f / 720.0f,
			0.1f,
			1000.0f);
}

/// <summary>
/// 更新
/// </summary>
void DebugCamera::Update(Input* input)
{
	// ------------------------------
	// 操作速度
	// ------------------------------

	const float mouseRotateSpeed = 0.003f;
	const float wheelSpeed = 0.03f;

	// ==================================================
	// 左クリック + マウス移動でピボット回転
	// ==================================================

	if (input->PushMouse(0))
	{
		// 今フレームのマウス移動量
		float mouseX =
			static_cast<float>(
				input->GetMouseMoveX());

		float mouseY =
			static_cast<float>(
				input->GetMouseMoveY());

		// 今フレームの回転角
		float rotateX = mouseY * mouseRotateSpeed;

		float rotateY = mouseX * mouseRotateSpeed;

		// ------------------------------
		// 追加回転行列
		// ------------------------------

		Matrix4x4 matRotDelta =
			MakeIdentity4x4();

		// X軸回転
		matRotDelta =
			Multiply(
				matRotDelta,
				MakeRotateXMatrix(
					rotateX));

		// Y軸回転
		matRotDelta =
			Multiply(
				matRotDelta,
				MakeRotateYMatrix(
					rotateY));

		// ------------------------------
		// 回転を累積
		// ------------------------------

		matRot_ =
			Multiply(
				matRotDelta,
				matRot_);
	}

	// ==================================================
	// ホイールで注視点との距離を変更
	// ==================================================

	float wheel =
		static_cast<float>(
			input->GetWheel());

	if (wheel != 0.0f)
	{
		distance_ -=
			wheel * wheelSpeed;

		// 注視点を突き抜けないようにする
		if (distance_ < 1.0f)
		{
			distance_ = 1.0f;
		}

		// 遠くなりすぎないようにする
		if (distance_ > 500.0f)
		{
			distance_ = 500.0f;
		}
	}

	// ==================================================
	// ピボット回転後のカメラ位置を計算
	// ==================================================

	// 注視点を中心としたカメラの相対位置
	Vector3 offset = {
		0.0f,
		0.0f,
		-distance_
	};

	// 累積回転行列で相対位置を回転
	offset =
		TransformNormal(
			offset,
			matRot_);

	// 注視点を基準にカメラ位置を計算
	Vector3 cameraPosition = {
		target_.x + offset.x,
		target_.y + offset.y,
		target_.z + offset.z
	};

	// ==================================================
	// ビュー行列を更新
	// ==================================================

	// カメラ位置の平行移動行列
	Matrix4x4 translateMatrix =
		MakeTranslateMatrix(
			cameraPosition);

	// 累積回転行列と平行移動を合成
	Matrix4x4 cameraMatrix =
		Multiply(
			matRot_,
			translateMatrix);

	// カメラWorld行列の逆行列をView行列にする
	viewMatrix_ =
		Inverse(
			cameraMatrix);
}