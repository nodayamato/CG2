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

// 点光源
struct PointLight
{
    // ライトの色
    float32_t4 color;
    // ライトの位置
    float32_t3 position;
    // ライトの明るさ
    float intensity;
    // ライトが届く最大距離
    float radius;
    // 減衰率
    float decay;
};

ConstantBuffer<PointLight> gPointLight : register(b3);

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
        // Cameraへの方向
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        
        // ----------------------
        // DirectionalLight
        // ----------------------
        
        // ライト方向
        float32_t3 directionalLightDirection = -gDirectionalLight.direction;
        // N・L
        float NdotLDirectional = dot(normal, directionalLightDirection);
        // Half Lambert
        float cosDirectional = pow(NdotLDirectional * 0.5f + 0.5f, 2.0f);
        // HalfVector
        float32_t3 halfVectorDirectional = normalize(directionalLightDirection + toEye);
        // N・H
        float NDotHDirectional = dot(normal, halfVectorDirectional);
        // Specular
        float specularPowDirectional = pow(saturate(NDotHDirectional), gMaterial.shininess);
        // 拡散反射
        float32_t3 diffuseDirectionalLight = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cosDirectional * gDirectionalLight.intensity;
        // 鏡面反射
        float32_t3 specularDirectionalLight = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPowDirectional * float32_t3(1.0f, 1.0f, 1.0f);

        // ----------------------
        // PointLight
        // ----------------------

        // PointLightまでの距離
        float distance = length(gPointLight.position - input.worldPosition);
        // 逆二乗による減衰
        float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);
        // 物体表面からPointLightへの向き
        float32_t3 pointLightDirection = normalize(gPointLight.position - input.worldPosition);
        // N・L
        float NdotLPoint = dot(normal, pointLightDirection);
        // Half Lambert
        float cosPoint = pow(NdotLPoint * 0.5f + 0.5f, 2.0f);
        // HalfVector
        float32_t3 halfVectorPoint = normalize(pointLightDirection + toEye);
        // N・H
        float NDotHPoint = dot(normal, halfVectorPoint);
        // 鏡面反射強度
        float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);
        // PointLight 拡散反射
        float32_t3 diffusePointLight = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * factor * gPointLight.intensity;
        // PointLight 鏡面反射
        float32_t3 specularPointLight = gPointLight.color.rgb * gPointLight.intensity * factor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);
        
        // ----------------------
        // 全部の光源の合計
        // ----------------------

        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight;
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