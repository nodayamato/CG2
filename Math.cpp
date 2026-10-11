#include "Math.h"
#include <cmath>

// Vector3を正規化
Vector3 Normalize(const Vector3& vector) {

	float length =
		std::sqrt(
			vector.x * vector.x +
			vector.y * vector.y +
			vector.z * vector.z
		);

	if (length == 0.0f) {

		return {
			0.0f,
			0.0f,
			0.0f
		};
	}

	return {
		vector.x / length,
		vector.y / length,
		vector.z / length
	};
}

// 4x4単位行列を作る
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result{};

	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	return result;
}

/// <summary>
/// 拡縮行列を作る
/// </summary>
/// <param name="scale">XYZ方向の拡縮率</param>
/// <returns>拡縮行列</returns>
Matrix4x4 MakeScaleMatrix(const Vector3& scale)
{
	Matrix4x4 result{};

	// X方向の拡縮
	result.m[0][0] = scale.x;

	// Y方向の拡縮
	result.m[1][1] = scale.y;

	// Z方向の拡縮
	result.m[2][2] = scale.z;

	// 同次座標用
	result.m[3][3] = 1.0f;

	return result;
}

/// <summary>
/// X軸回転行列を作る
/// </summary>
/// <param name="radian">回転角度。単位はラジアン</param>
/// <returns>X軸回転行列</returns>
Matrix4x4 MakeRotateXMatrix(float radian)
{
	Matrix4x4 result = MakeIdentity4x4();

	const float cosine = cosf(radian);
	const float sine = sinf(radian);

	result.m[1][1] = cosine;
	result.m[1][2] = sine;

	result.m[2][1] = -sine;
	result.m[2][2] = cosine;

	return result;
}

/// <summary>
/// Y軸回転行列を作る
/// </summary>
/// <param name="radian">回転角度。単位はラジアン</param>
/// <returns>Y軸回転行列</returns>
Matrix4x4 MakeRotateYMatrix(float radian)
{
	Matrix4x4 result = MakeIdentity4x4();

	const float cosine = cosf(radian);
	const float sine = sinf(radian);

	result.m[0][0] = cosine;
	result.m[0][2] = -sine;

	result.m[2][0] = sine;
	result.m[2][2] = cosine;

	return result;
}

/// <summary>
/// Z軸回転行列を作る
/// </summary>
/// <param name="radian">回転角度。単位はラジアン</param>
/// <returns>Z軸回転行列</returns>
Matrix4x4 MakeRotateZMatrix(float radian)
{
	Matrix4x4 result{};

	// cosとsinを計算
	const float cosine = std::cos(radian);
	const float sine = std::sin(radian);

	// Z軸回転行列
	result.m[0][0] = cosine;
	result.m[0][1] = sine;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = -sine;
	result.m[1][1] = cosine;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1.0f;
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

/// <summary>
/// 平行移動行列を作る
/// </summary>
/// <param name="translate">XYZ方向の移動量</param>
/// <returns>平行移動行列</returns>
Matrix4x4 MakeTranslateMatrix(const Vector3& translate)
{
	Matrix4x4 result = MakeIdentity4x4();

	// 行ベクトル形式なので、平行移動は4行目に入れる
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;

	return result;
}

// アフィン変換行列を作成する
Matrix4x4 MakeAffineMatrix(
	const Vector3& scale,
	const Vector3& rotate,
	const Vector3& translate)
{
	Matrix4x4 result{};

	float cosX = cosf(rotate.x);
	float sinX = sinf(rotate.x);
	float cosY = cosf(rotate.y);
	float sinY = sinf(rotate.y);
	float cosZ = cosf(rotate.z);
	float sinZ = sinf(rotate.z);

	result.m[0][0] = scale.x * (cosY * cosZ);
	result.m[0][1] = scale.x * (cosY * sinZ);
	result.m[0][2] = scale.x * (-sinY);
	result.m[0][3] = 0.0f;

	result.m[1][0] = scale.y * (sinX * sinY * cosZ - cosX * sinZ);
	result.m[1][1] = scale.y * (sinX * sinY * sinZ + cosX * cosZ);
	result.m[1][2] = scale.y * (sinX * cosY);
	result.m[1][3] = 0.0f;

	result.m[2][0] = scale.z * (cosX * sinY * cosZ + sinX * sinZ);
	result.m[2][1] = scale.z * (cosX * sinY * sinZ - sinX * cosZ);
	result.m[2][2] = scale.z * (cosX * cosY);
	result.m[2][3] = 0.0f;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1.0f;

	return result;
}

// 行列の掛け算
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2)
{
	Matrix4x4 result{};

	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			result.m[y][x] =
				m1.m[y][0] * m2.m[0][x] +
				m1.m[y][1] * m2.m[1][x] +
				m1.m[y][2] * m2.m[2][x] +
				m1.m[y][3] * m2.m[3][x];
		}
	}

	return result;
}

// 逆行列を求める
Matrix4x4 Inverse(const Matrix4x4& m)
{
	Matrix4x4 result{};

	result.m[0][0] = m.m[0][0];
	result.m[0][1] = m.m[1][0];
	result.m[0][2] = m.m[2][0];

	result.m[1][0] = m.m[0][1];
	result.m[1][1] = m.m[1][1];
	result.m[1][2] = m.m[2][1];

	result.m[2][0] = m.m[0][2];
	result.m[2][1] = m.m[1][2];
	result.m[2][2] = m.m[2][2];

	result.m[3][3] = 1.0f;

	result.m[3][0] =
		-(m.m[3][0] * result.m[0][0] +
			m.m[3][1] * result.m[1][0] +
			m.m[3][2] * result.m[2][0]);

	result.m[3][1] =
		-(m.m[3][0] * result.m[0][1] +
			m.m[3][1] * result.m[1][1] +
			m.m[3][2] * result.m[2][1]);

	result.m[3][2] =
		-(m.m[3][0] * result.m[0][2] +
			m.m[3][1] * result.m[1][2] +
			m.m[3][2] * result.m[2][2]);

	return result;
}

// 透視投影行列を作成する
Matrix4x4 MakePerspectiveFovMatrix(
	float fovY,
	float aspectRatio,
	float nearClip,
	float farClip)
{
	Matrix4x4 result{};

	float f = 1.0f / tanf(fovY / 2.0f);

	result.m[0][0] = f / aspectRatio;
	result.m[1][1] = f;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);

	return result;
}

// 正射影行列を作成する
Matrix4x4 MakeOrthographicMatrix(
	float left,
	float top,
	float right,
	float bottom,
	float nearClip,
	float farClip)
{
	Matrix4x4 result{};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;

	return result;
}