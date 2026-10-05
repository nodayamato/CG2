#include "Particle.hlsli"

// マテリアル
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

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

    // UV座標を変換する
    float32_t4 transformedUV =
        mul(
            float32_t4(
                input.texcoord,
                0.0f,
                1.0f),
            gMaterial.uvTransform);

    // 変換後のUVでTextureを読む
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // マテリアルの色とTextureの色を掛け合わせる
    output.color = gMaterial.color * textureColor;

    // 最終的に完全透明なら破棄
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}