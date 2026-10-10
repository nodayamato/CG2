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
	AddParticles(3);

	numInstance_ = 0;
}

// -----------------
// Particle追加
// -----------------
void ParticleSystem::AddParticles(uint32_t count) {

	for (uint32_t index = 0; index < count; ++index) {

		particles_.push_back(
			MakeNewParticle(
				{ 0.0f, 0.0f, 0.0f }
			)
		);
	}
}

// -----------------
// EmitterからParticle生成
// -----------------
void ParticleSystem::Emit(const Emitter& emitter) {

	for (uint32_t count = 0; count < emitter.count; ++count) {

		particles_.push_back(
			MakeNewParticle(
				emitter.transform.translate
			)
		);
	}
}

// -----------------
// Particle生成
// -----------------
Particle ParticleSystem::MakeNewParticle(const Vector3& translate) {

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
	// Emitterを基準にランダム配置
	// -----------------
	Vector3 randomTranslate = {
		distribution(randomEngine_),
		distribution(randomEngine_),
		distribution(randomEngine_)
	};

	particle.transform.translate = {
		translate.x + randomTranslate.x,
		translate.y + randomTranslate.y,
		translate.z + randomTranslate.z
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
	const Matrix4x4& billboardMatrix,
	ParticleForGPU* instancingData,
	float deltaTime,
	bool useBillboard) {

	// -----------------
	// 描画数リセット
	// -----------------
	numInstance_ = 0;

	// -----------------
	// Particle更新
	// -----------------
	for (std::list<Particle>::iterator particleIterator = particles_.begin(); particleIterator != particles_.end();) {

		// -----------------
		// 寿命切れなら削除
		// -----------------
		if (particleIterator->lifeTime <=
			particleIterator->currentTime) {

			particleIterator =
				particles_.erase(
					particleIterator
				);

			continue;
		}

		// -----------------
		// 経過時間
		// -----------------
		particleIterator->currentTime +=
			deltaTime;

		// -----------------
		// 位置更新
		// -----------------
		particleIterator->transform.translate.x +=
			particleIterator->velocity.x *
			deltaTime;

		particleIterator->transform.translate.y +=
			particleIterator->velocity.y *
			deltaTime;

		particleIterator->transform.translate.z +=
			particleIterator->velocity.z *
			deltaTime;

		// -----------------
		// Scale
		// -----------------
		float t =
			particleIterator->currentTime /
			particleIterator->lifeTime;

		float scale =
			1.0f - t;

		particleIterator->transform.scale = {
			scale,
			scale,
			scale
		};

		// -----------------
		// Rotate
		// -----------------
		particleIterator->transform.rotate.z +=
			1.0f * deltaTime;

		// -----------------
		// GPU最大数以内だけ描画データ作成
		// -----------------
		if (numInstance_ <
			kNumMaxInstance) {

			Matrix4x4 worldMatrix{};

			// -----------------
			// Billboardあり
			// -----------------
			if (useBillboard) {

				Matrix4x4 scaleMatrix =
					MakeScaleMatrix(
						particleIterator->
						transform.scale
					);

				Matrix4x4 translateMatrix =
					MakeTranslateMatrix(
						particleIterator->
						transform.translate
					);

				worldMatrix =
					Multiply(
						Multiply(
							scaleMatrix,
							billboardMatrix
						),
						translateMatrix
					);
			}

			// -----------------
			// Billboardなし
			// -----------------
			else {

				worldMatrix =
					MakeAffineMatrix(
						particleIterator->
						transform.scale,

						particleIterator->
						transform.rotate,

						particleIterator->
						transform.translate
					);
			}

			// -----------------
			// WVP
			// -----------------
			Matrix4x4
				worldViewProjectionMatrix =
				Multiply(
					worldMatrix,
					viewProjectionMatrix
				);

			// -----------------
			// GPUへ送る
			// -----------------
			instancingData[numInstance_].WVP =
				worldViewProjectionMatrix;

			instancingData[numInstance_].World =
				worldMatrix;

			instancingData[numInstance_].color =
				particleIterator->color;

			// -----------------
			// Alpha
			// -----------------
			float alpha =
				1.0f -
				(
					particleIterator->
					currentTime /

					particleIterator->
					lifeTime
					);

			instancingData[numInstance_].
				color.w = alpha;

			// -----------------
			// Dissolve
			// -----------------
			float dissolve =
				particleIterator->
				currentTime /

				particleIterator->
				lifeTime;

			instancingData[numInstance_].
				dissolveThreshold =
				dissolve;

			// -----------------
			// 描画数を増やす
			// -----------------
			++numInstance_;
		}

		// -----------------
		// 次のParticleへ
		// -----------------
		++particleIterator;
	}
}