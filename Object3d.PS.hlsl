#include "Object3d.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct Material
{
    float32_t4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // TextureをSampling
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // Materialの色とTextureの色を乗算
    output.color = gMaterial.color * textureColor;

    return output;
}