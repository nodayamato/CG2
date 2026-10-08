#pragma once
#include "Math.h"
#include <array>
#include <cstdint>
#include <random>

class ParticleSystem {
public:

	// -----------------
	// 最大Particle数
	// -----------------
	static constexpr uint32_t kNumMaxInstance = 10;

	// -----------------
	// 初期化
	// -----------------
	void Initialize();

	// -----------------
	// 更新
	// -----------------
	void Update(
		const Matrix4x4& viewProjectionMatrix,
		ParticleForGPU* instancingData,
		float deltaTime
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
	Particle MakeNewParticle();

private:

	// -----------------
	// Particle本体
	// -----------------
	std::array<Particle, kNumMaxInstance> particles_{};

	// -----------------
	// 乱数生成器
	// -----------------
	std::mt19937 randomEngine_;

	// -----------------
	// 今フレームで描画する数
	// -----------------
	uint32_t numInstance_ = 0;
};