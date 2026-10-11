#include "Object3d.hlsli"

// マテリアル
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t shininess;
    float32_t2 padding;
    float32_t4x4 uvTransform;
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

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

// カメラ
struct Camera
{
    float32_t3 worldPosition;
    float32_t padding;
};

ConstantBuffer<Camera> gCamera : register(b2);

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
                1.0f
            ),
            gMaterial.uvTransform);

    // 変換後のUVでTextureを読む
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 完全に透明なPixelを破棄
    if (textureColor.a == 0.0f)
    {
        discard;
    }

    // Lightingを有効にしている場合
    if (gMaterial.enableLighting != 0)
    {
        // 法線
        float32_t3 normal = normalize(input.normal);
        // 法線と光源の方向の内積
        float NdotL = dot(normal, -gDirectionalLight.direction);
        // ランバートの法則
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        // Cameraへの方向
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        // 反射ベクトル
        float32_t3 reflectLight = reflect(gDirectionalLight.direction, normal);
        // 鏡面反射の強さ
        float RdotE = dot(reflectLight, toEye);
        // 鏡面反射の強さを0.0～1.0に制限して、shininess乗する
        float specularPow = pow(saturate(RdotE), gMaterial.shininess);
        // 拡散反射
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        // 鏡面反射
        float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        // 合成
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    // 最終的に完全透明なら破棄
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}