#ifndef SPRITE_HLSLI
#define SPRITE_HLSLI

// =====================================
// Sprite共通の構造体
// =====================================

// VertexShaderからPixelShaderへ渡すデータ
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

#endif