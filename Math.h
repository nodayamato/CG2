#pragma once

// 3次元ベクトル
struct Vector3 {
	float x;
	float y;
	float z;
};

// 4次元ベクトル
struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

// 4x4行列
struct Matrix4x4 {
	float m[4][4];
};

// 3Dオブジェクトの変換情報
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// 単位行列を作る
Matrix4x4 MakeIdentity4x4();

// アフィン変換行列を作成する
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

// 行列の掛け算
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

// 逆行列を求める
Matrix4x4 Inverse(const Matrix4x4& m);

// 透視投影行列を作成する
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);