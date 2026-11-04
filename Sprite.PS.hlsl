#include "Sprite.hlsli"

// =====================================
// Sprite PixelShader
// =====================================

// Material
cbuffer Material : register(b0)
{
    float4 color;
};

// Texture
Texture2D<float4> gTexture : register(t0);

// Sampler
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{

    float4 textureColor =
        gTexture.Sample(gSampler, input.texcoord);

    float4 outputColor = textureColor * color;

    clip(outputColor.a - 0.001f);

    return outputColor;
}