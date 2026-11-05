#pragma once

#include "Math.h"
#include "ParticleSystem.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class DirectXCommon;
class SrvManager;

// =====================================
// Particle管理クラス
// =====================================
class ParticleManager {
public:

    // 初期化
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    // パーティクルグループを作成
    void CreateParticleGroup(
        const std::string& groupName,
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
    );

    // グループを指定して発生
    void Emit(
        const std::string& groupName,
        const Emitter& emitter
    );

    // Particle発生
    void Emit(const Emitter& emitter);

    // AccelerationField設定
    void SetAccelerationField(
        const AccelerationField& field
    );

    // 更新
    void Update(
        const Matrix4x4& viewProjectionMatrix,
        const Matrix4x4& billboardMatrix,
        float deltaTime,
        bool useBillboard,
        bool useAccelerationField
    );

    // UV行列の設定
    void SetUVTransform(
        const Matrix4x4& uvTransform
    );

    // 描画用テクスチャハンドル設定
    void SetTextureHandle(
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
    );

    // Particle描画
    void Draw();

    // 描画するParticle数
    uint32_t GetNumInstance() const;

    // 終了処理
    void Finalize();

private:

    // RootSignature生成
    void CreateRootSignature();

    // PSO生成
    void CreateGraphicsPipeline();

    // 頂点バッファ生成
    void CreateVertexResource();

    // インスタンシング用リソース生成
    void CreateInstancingResource();

    // Material生成
    void CreateMaterialResource();

private:

    // ------------------------------------
    // パーティクルグループ
	// ------------------------------------
    struct ParticleGroup {

        // グループごとのパーティクル
        ParticleSystem particleSystem;

        // インスタンシング用GPUリソース
        Microsoft::WRL::ComPtr<ID3D12Resource>
            instancingResource;

        // 書き込み先
        ParticleForGPU* instancingData = nullptr;

        // インスタンシング用SRV番号
        uint32_t srvIndex = 0;

        // インスタンシング用GPUハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE
            instancingSrvHandleGPU{};

        // 使用する画像のGPUハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE
            textureSrvHandleGPU{};
    };

    // グループ一覧
    std::unordered_map<
        std::string,
        std::unique_ptr<ParticleGroup>
    > particleGroups_;

    // AccelerationFieldを保持
    AccelerationField accelerationField_{};

    // AccelerationFieldが設定されたか
    bool hasAccelerationField_ = false;

    // 外部クラス（所有しない）
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Particleの動作管理
    ParticleSystem particleSystem_;

    // RootSignature
    Microsoft::WRL::ComPtr<ID3D12RootSignature>
        rootSignature_;

    // PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>
        pipelineState_;

    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource>
        vertexResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // 頂点数
    uint32_t vertexCount_ = 0;

    // インスタンシング
    Microsoft::WRL::ComPtr<ID3D12Resource>
        instancingResource_;

    ParticleForGPU* instancingData_ = nullptr;

    // Material
    Microsoft::WRL::ComPtr<ID3D12Resource>
        materialResource_;

    Material* materialData_ = nullptr;

    // ParticleのStructuredBuffer用SRV
    D3D12_GPU_DESCRIPTOR_HANDLE
        instancingSrvHandleGPU_{};

    // 画像用SRV
    D3D12_GPU_DESCRIPTOR_HANDLE
        textureSrvHandleGPU_{};

    // 今回は既存の予約番号3を使用
    static constexpr uint32_t kInstancingSrvIndex = 3;
};