#include "Object3d.hlsli"

// マテリアル
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};

ConstantBuffer<Material> gMaterial : register(b0);

// 平行光源
struct DirectionalLight
{
	// ライトの色
    float32_t4 color;

	// ライトの向き
    float32_t3 direction;

	// ライトの明るさ
    float intensity;
};

ConstantBuffer<DirectionalLight>
	gDirectionalLight : register(b1);

// Texture
Texture2D<float32_t4> gTexture : register(t0);

// Sampler
SamplerState gSampler : register(s0);

// PixelShaderの出力
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

	// Textureから色を取得
    float32_t4 textureColor =
		gTexture.Sample(gSampler, input.texcoord);

	// Lightingを有効にしている場合
    if (gMaterial.enableLighting != 0)
    {
    // 法線とライト方向の内積
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);

    // Half Lambert
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}