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

// 頂点データ
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// 4x4行列
struct Matrix4x4 {
	float m[4][4];
};

// マテリアル情報
struct Material {
	Vector4 color;
	int enableLighting;
	// ConstantBufferのアライメントを合わせるための余白
	float padding[3];
	// UV座標変換行列
	Matrix4x4 uvTransform;
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

// パーティクルの情報
struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

// GPU用パーティクルデータ
struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
	// Dissolveの進み具合
	float dissolveThreshold;
	// アラインメント調整
	float padding[3];
};

// 4x4単位行列を作る
Matrix4x4 MakeIdentity4x4();

// 拡縮行列を作る
Matrix4x4 MakeScaleMatrix(const Vector3& scale);

// X軸回転行列を作る
Matrix4x4 MakeRotateXMatrix(float radian);

// Y軸回転行列を作る
Matrix4x4 MakeRotateYMatrix(float radian);

// Z軸回転行列を作る
Matrix4x4 MakeRotateZMatrix(float radian);

// 平行移動行列を作る
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

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