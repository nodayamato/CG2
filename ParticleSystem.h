#pragma once
#include "Math.h"
#include <list>
#include <cstdint>
#include <random>

// -----------------
// Emitter
// -----------------
struct Emitter {
	Transform transform; // エミッタのTransform
	uint32_t count; // 1回の発生数
	float frequency; // 発生頻度
	float frequencyTime; // 経過時間
};

class ParticleSystem {
public:

	// -----------------
	// 最大Particle数
	// -----------------
	static constexpr uint32_t kNumMaxInstance = 100;

	// -----------------
	// 初期化
	// -----------------
	void Initialize();

	// -----------------
	// Particle追加
	// -----------------
	void AddParticles(uint32_t count);

	// -----------------
	// EmitterからParticle生成
	// -----------------
	void Emit(const Emitter& emitter);

	// -----------------
	// 更新
	// -----------------
	void Update(
		const Matrix4x4& viewProjectionMatrix,
		const Matrix4x4& billboardMatrix,
		ParticleForGPU* instancingData,
		float deltaTime,
		bool useBillboard
	);

	// -----------------
	// 描画するParticle数を取得
	// -----------------
	uint32_t GetNumInstance() const {
		return numInstance_;
	}

private:

	// -----------------
	// Particle生成
	// -----------------
	Particle MakeNewParticle(const Vector3& translate);

private:

	// -----------------
	// Particle本体
	// -----------------
	std::list<Particle> particles_;

	// -----------------
	// 乱数生成器
	// -----------------
	std::mt19937 randomEngine_;

	// -----------------
	// 今フレームで描画する数
	// -----------------
	uint32_t numInstance_ = 0;
};