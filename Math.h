#pragma once

// 2次元ベクトル
struct Vector2 {
	float x;
	float y;
};

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

// マテリアル情報
struct Material {
	Vector4 color;
	int enableLighting;
};

// 頂点データ
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// 平行光源
struct DirectionalLight
{
	// ライトの色
	Vector4 color;
	// ライトの向き
	Vector3 direction;
	// ライトの明るさ
	float intensity;
};

// 4x4行列
struct Matrix4x4 {
	float m[4][4];
};

// ワールド行列、ビュー行列、射影行列をまとめた構造体
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
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

// 正射影行列を作成する
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);