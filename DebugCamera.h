#pragma once
#include "Math.h"
#include "Input.h"

class DebugCamera
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update(Input* input);

	/// <summary>
	/// ビュー行列を取得
	/// </summary>
	const Matrix4x4& GetViewMatrix() const
	{
		return viewMatrix_;
	}

	/// <summary>
	/// 射影行列を取得
	/// </summary>
	const Matrix4x4& GetProjectionMatrix() const
	{
		return projectionMatrix_;
	}

private:
	// 累積回転行列
	Matrix4x4 matRot_{};

	// 注視点
	Vector3 target_ = {
		0.0f,
		0.0f,
		0.0f
	};

	// 注視点からカメラまでの距離
	float distance_ = 50.0f;

	// ビュー行列
	Matrix4x4 viewMatrix_{};

	// 射影行列
	Matrix4x4 projectionMatrix_{};
};