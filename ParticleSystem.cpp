#include "ParticleSystem.h"

// -----------------
// 初期化
// -----------------
void ParticleSystem::Initialize() {

	// -----------------
	// 乱数生成器初期化
	// -----------------
	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	// -----------------
	// Particle生成
	// -----------------
	for (uint32_t index = 0;
		index < kNumMaxInstance;
		++index) {

		particles_[index] =
			MakeNewParticle();
	}

	numInstance_ = 0;
}

// -----------------
// Particle生成
// -----------------
Particle ParticleSystem::MakeNewParticle() {

	// -----------------
	// 乱数
	// -----------------
	std::uniform_real_distribution<float>
		distribution(-1.0f, 1.0f);

	std::uniform_real_distribution<float>
		distColor(0.0f, 1.0f);

	std::uniform_real_distribution<float>
		distTime(1.0f, 3.0f);

	Particle particle{};

	// -----------------
	// Scale
	// -----------------
	particle.transform.scale = {
		1.0f,
		1.0f,
		1.0f
	};

	// -----------------
	// Rotate
	// -----------------
	particle.transform.rotate = {
		0.0f,
		0.0f,
		0.0f
	};

	// -----------------
	// 初期位置
	// -----------------
	particle.transform.translate = {
		distribution(randomEngine_),
		distribution(randomEngine_),
		distribution(randomEngine_)
	};

	// -----------------
	// 速度
	// -----------------
	particle.velocity = {
		distribution(randomEngine_),
		distribution(randomEngine_),
		distribution(randomEngine_)
	};

	// -----------------
	// 色
	// -----------------
	particle.color = {
		distColor(randomEngine_),
		distColor(randomEngine_),
		distColor(randomEngine_),
		1.0f
	};

	// -----------------
	// 寿命
	// -----------------
	particle.lifeTime =
		distTime(randomEngine_);

	// -----------------
	// 経過時間
	// -----------------
	particle.currentTime = 0.0f;

	return particle;
}

// -----------------
// 更新
// -----------------
void ParticleSystem::Update(
	const Matrix4x4& viewProjectionMatrix,
	ParticleForGPU* instancingData,
	float deltaTime) {

	// -----------------
	// 描画数をリセット
	// -----------------
	numInstance_ = 0;

	// -----------------
	// Particle更新
	// -----------------
	for (uint32_t index = 0;
		index < kNumMaxInstance;
		++index) {

		// -----------------
		// 寿命切れ
		// -----------------
		if (particles_[index].lifeTime <
			particles_[index].currentTime) {

			// 新しいParticleを生成
			particles_[index] = MakeNewParticle();
		}

		// -----------------
		// 経過時間
		// -----------------
		particles_[index].currentTime +=
			deltaTime;

		// -----------------
		// 位置更新
		// -----------------
		particles_[index].transform.translate.x +=
			particles_[index].velocity.x *
			deltaTime;

		particles_[index].transform.translate.y +=
			particles_[index].velocity.y *
			deltaTime;

		particles_[index].transform.translate.z +=
			particles_[index].velocity.z *
			deltaTime;

		// -----------------
		// Scaleを時間で変化
		// -----------------
		
		// 0.0 ～ 1.0 の経過割合
		float t =
			particles_[index].currentTime /
			particles_[index].lifeTime;

		// 寿命に近づくほど小さくする
		float scale =
			1.0f - t;

		particles_[index].transform.scale = {
			scale,
			scale,
			scale
		};

		// -----------------
		// Rotateを時間で変化
		// -----------------

		particles_[index].transform.rotate.z +=
			1.0f * deltaTime;

		// -----------------
		// World
		// -----------------
		Matrix4x4 worldMatrix =
			MakeAffineMatrix(
				particles_[index].transform.scale,
				particles_[index].transform.rotate,
				particles_[index].transform.translate
			);

		// -----------------
		// WVP
		// -----------------
		Matrix4x4 worldViewProjectionMatrix =
			Multiply(
				worldMatrix,
				viewProjectionMatrix
			);

		// -----------------
		// GPUへ渡す
		// -----------------
		instancingData[numInstance_].WVP =
			worldViewProjectionMatrix;

		instancingData[numInstance_].World =
			worldMatrix;

		instancingData[numInstance_].color =
			particles_[index].color;

		// -----------------
		// 徐々に透明にする
		// -----------------
		float alpha =
			1.0f -
			(
				particles_[index].currentTime /
				particles_[index].lifeTime
				);

		instancingData[numInstance_].color.w = alpha;

		// -----------------
		// Dissolve
		// -----------------

		// 0.0 ～ 1.0
		float dissolve =
			particles_[index].currentTime /
			particles_[index].lifeTime;

		instancingData[numInstance_].dissolveThreshold = dissolve;

		// -----------------
		// 描画数を増やす
		// -----------------
		++numInstance_;
	}
}