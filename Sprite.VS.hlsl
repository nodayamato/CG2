#include "Sprite.hlsli"

// =====================================
// Sprite VertexShader
// =====================================

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// WVP行列
cbuffer TransformationMatrix : register(b0)
{
    row_major float4x4 WVP;
    row_major float4x4 World;
    row_major float4x4 WorldInverseTranspose;
};

VertexShaderOutput main(VertexShaderInput input)
{

    VertexShaderOutput output;

    output.position = mul(input.position, WVP);
    output.texcoord = input.texcoord;

    return output;
}