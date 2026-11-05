#include "ParticleManager.h"

#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Logger.h"

#include <cassert>
#include <cstring>
#include <vector>

// =====================================
// 初期化
// =====================================
void ParticleManager::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager
) {
    assert(dxCommon != nullptr);
    assert(srvManager != nullptr);
    assert(dxCommon->GetDevice() != nullptr);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // Particleの動作管理を初期化
    particleSystem_.Initialize();

    // 描画に必要なリソースを作る
    CreateRootSignature();
    CreateGraphicsPipeline();
    CreateVertexResource();
    CreateInstancingResource();
    CreateMaterialResource();
}

// =====================================
// Particleを発生させる
// =====================================
void ParticleManager::Emit(
    const Emitter& emitter
) {
    particleSystem_.Emit(emitter);
}

void ParticleManager::SetAccelerationField(
    const AccelerationField& field
) {
    accelerationField_ = field;
    hasAccelerationField_ = true;

    // 既存のParticleSystem
    particleSystem_.SetAccelerationField(
        field
    );

    // 追加したグループにも反映
    for (auto& [name, group] : particleGroups_) {
        group->particleSystem.SetAccelerationField(
            field
        );
    }
}

// =====================================
// 更新
// =====================================
void ParticleManager::Update(
    const Matrix4x4& viewProjectionMatrix,
    const Matrix4x4& billboardMatrix,
    float deltaTime,
    bool useBillboard,
    bool useAccelerationField
) {
    assert(instancingData_ != nullptr);

    // ---------------------------------
    // 既存グループ
    // ---------------------------------
    particleSystem_.Update(
        viewProjectionMatrix,
        billboardMatrix,
        instancingData_,
        deltaTime,
        useBillboard,
        useAccelerationField
    );

    // ---------------------------------
    // 追加グループ
    // ---------------------------------
    for (auto& [name, group] : particleGroups_) {

        assert(group->instancingData != nullptr);

        group->particleSystem.Update(
            viewProjectionMatrix,
            billboardMatrix,
            group->instancingData,
            deltaTime,
            useBillboard,
            useAccelerationField
        );
    }
}

// =====================================
// UV行列設定
// =====================================
void ParticleManager::SetUVTransform(
    const Matrix4x4& uvTransform
) {
    assert(materialData_ != nullptr);

    materialData_->uvTransform = uvTransform;
}

// =====================================
// 描画用テクスチャを設定
// =====================================
void ParticleManager::SetTextureHandle(
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
) {
    textureSrvHandleGPU_ = textureHandle;
}

// =====================================
// 描画数取得
// =====================================
uint32_t ParticleManager::GetNumInstance() const {
    return particleSystem_.GetNumInstance();
}

// =====================================
// 頂点バッファ生成
// =====================================
void ParticleManager::CreateVertexResource() {

    // 板ポリを三角形2枚で作成
    const VertexData vertices[] = {
        {
            {-1.0f, 1.0f, 0.0f, 1.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f}
        },
        {
            {1.0f, 1.0f, 0.0f, 1.0f},
            {1.0f, 0.0f},
            {0.0f, 0.0f, -1.0f}
        },
        {
            {-1.0f, -1.0f, 0.0f, 1.0f},
            {0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f}
        },
        {
            {-1.0f, -1.0f, 0.0f, 1.0f},
            {0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f}
        },
        {
            {1.0f, 1.0f, 0.0f, 1.0f},
            {1.0f, 0.0f},
            {0.0f, 0.0f, -1.0f}
        },
        {
            {1.0f, -1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f},
            {0.0f, 0.0f, -1.0f}
        }
    };

    vertexCount_ = 6;

    const size_t bufferSize =
        sizeof(vertices);

    vertexResource_ =
        dxCommon_->CreateBufferResource(
            bufferSize
        );

    assert(vertexResource_ != nullptr);

    // 頂点データを書き込む
    void* mappedData = nullptr;

    HRESULT hr = vertexResource_->Map(
        0,
        nullptr,
        &mappedData
    );

    assert(SUCCEEDED(hr));

    std::memcpy(
        mappedData,
        vertices,
        bufferSize
    );

    vertexResource_->Unmap(0, nullptr);

    // VertexBufferView設定
    vertexBufferView_.BufferLocation =
        vertexResource_->GetGPUVirtualAddress();

    vertexBufferView_.SizeInBytes =
        static_cast<UINT>(bufferSize);

    vertexBufferView_.StrideInBytes =
        sizeof(VertexData);
}

// =====================================
// インスタンシング用リソース生成
// =====================================
void ParticleManager::CreateInstancingResource() {

    const uint32_t maxInstance =
        ParticleSystem::kNumMaxInstance;

    instancingResource_ =
        dxCommon_->CreateBufferResource(
            sizeof(ParticleForGPU) * maxInstance
        );

    assert(instancingResource_ != nullptr);

    HRESULT hr = instancingResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(
            &instancingData_
            )
    );

    assert(SUCCEEDED(hr));

    // 初期化
    for (uint32_t i = 0; i < maxInstance; ++i) {

        instancingData_[i].WVP =
            MakeIdentity4x4();

        instancingData_[i].World =
            MakeIdentity4x4();

        instancingData_[i].color = {
            1.0f, 1.0f, 1.0f, 1.0f
        };

        instancingData_[i].dissolveThreshold =
            0.0f;
    }

    // SrvManagerを使用してSRVを作成
    srvManager_->CreateSRVForStructuredBuffer(
        kInstancingSrvIndex,
        instancingResource_.Get(),
        maxInstance,
        sizeof(ParticleForGPU)
    );

    instancingSrvHandleGPU_ =
        srvManager_->GetGPUDescriptorHandle(
            kInstancingSrvIndex
        );
}

// =====================================
// Materialリソース生成
// =====================================
void ParticleManager::CreateMaterialResource() {

    // CBV用サイズは256バイトに整列
    const size_t bufferSize =
        (sizeof(Material) + 255) & ~size_t(255);

    materialResource_ =
        dxCommon_->CreateBufferResource(
            bufferSize
        );

    assert(materialResource_ != nullptr);

    HRESULT hr = materialResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(
            &materialData_
            )
    );

    assert(SUCCEEDED(hr));

    materialData_->color = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    materialData_->enableLighting = false;
    materialData_->shininess = 32.0f;
    materialData_->uvTransform =
        MakeIdentity4x4();
}

// =====================================
// RootSignature生成
// =====================================
void ParticleManager::CreateRootSignature() {

    ID3D12Device* device =
        dxCommon_->GetDevice();

    assert(device != nullptr);

    D3D12_ROOT_SIGNATURE_DESC desc{};

    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Instancing用SRV：VSのt0
    D3D12_DESCRIPTOR_RANGE instancingRange{};
    instancingRange.RangeType =
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    instancingRange.BaseShaderRegister = 0;
    instancingRange.NumDescriptors = 1;
    instancingRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Texture用SRV：PSのt0
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.RangeType =
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3]{};

    // Material：PSのb0
    rootParameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // Instancing：VSのt0
    rootParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.pDescriptorRanges =
        &instancingRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // Texture：PSのt0
    rootParameters[2].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges =
        &textureRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    // Sampler
    D3D12_STATIC_SAMPLER_DESC sampler{};

    sampler.Filter =
        D3D12_FILTER_MIN_MAG_MIP_LINEAR;

    sampler.AddressU =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    sampler.AddressV =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    sampler.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    sampler.ComparisonFunc =
        D3D12_COMPARISON_FUNC_NEVER;

    sampler.MaxLOD =
        D3D12_FLOAT32_MAX;

    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    desc.pStaticSamplers = &sampler;
    desc.NumStaticSamplers = 1;

    // シリアライズ
    Microsoft::WRL::ComPtr<ID3DBlob>
        signatureBlob;

    Microsoft::WRL::ComPtr<ID3DBlob>
        errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr)) {

        if (errorBlob != nullptr) {
            Logger::Log(
                static_cast<const char*>(
                    errorBlob->GetBufferPointer()
                    )
            );
        }

        assert(false);
        return;
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(
            rootSignature_.GetAddressOf()
        )
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// PSO生成
// =====================================
void ParticleManager::CreateGraphicsPipeline() {

    ID3D12Device* device =
        dxCommon_->GetDevice();

    assert(device != nullptr);
    assert(rootSignature_ != nullptr);

    // Particle用シェーダー
    auto vertexShader =
        dxCommon_->CompileShader(
            L"Particle.VS.hlsl",
            L"vs_6_0"
        );

    auto pixelShader =
        dxCommon_->CompileShader(
            L"Particle.PS.hlsl",
            L"ps_6_0"
        );

    assert(vertexShader != nullptr);
    assert(pixelShader != nullptr);

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElements[3]{};

    inputElements[0].SemanticName = "POSITION";
    inputElements[0].Format =
        DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElements[0].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].Format =
        DXGI_FORMAT_R32G32_FLOAT;
    inputElements[1].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    inputElements[2].SemanticName = "NORMAL";
    inputElements[2].Format =
        DXGI_FORMAT_R32G32B32_FLOAT;
    inputElements[2].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayout{};

    inputLayout.pInputElementDescs = inputElements;
    inputLayout.NumElements = _countof(inputElements);

    // Rasterizer
    D3D12_RASTERIZER_DESC rasterizer{};

    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    // DepthStencil
    D3D12_DEPTH_STENCIL_DESC depthStencil{};

    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask =
        D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencil.DepthFunc =
        D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 通常アルファブレンド
    D3D12_BLEND_DESC blend{};

    D3D12_RENDER_TARGET_BLEND_DESC& target =
        blend.RenderTarget[0];

    target.BlendEnable = TRUE;

    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend =
        D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;

    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_ZERO;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    target.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

    psoDesc.pRootSignature =
        rootSignature_.Get();

    psoDesc.InputLayout = inputLayout;

    psoDesc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };

    psoDesc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };

    psoDesc.RasterizerState = rasterizer;
    psoDesc.DepthStencilState = depthStencil;
    psoDesc.BlendState = blend;

    psoDesc.NumRenderTargets = 1;

    psoDesc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    psoDesc.DSVFormat =
        DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask =
        D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr =
        device->CreateGraphicsPipelineState(
            &psoDesc,
            IID_PPV_ARGS(
                pipelineState_.GetAddressOf()
            )
        );

    assert(SUCCEEDED(hr));
}

// =====================================
// Particle描画
// =====================================
void ParticleManager::Draw() {

    assert(dxCommon_ != nullptr);
    assert(rootSignature_ != nullptr);
    assert(pipelineState_ != nullptr);
    assert(materialResource_ != nullptr);

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();

    assert(commandList != nullptr);

    // ---------------------------------
    // 共通の描画設定
    // ---------------------------------

    commandList->SetGraphicsRootSignature(
        rootSignature_.Get()
    );

    commandList->SetPipelineState(
        pipelineState_.Get()
    );

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView_
    );

    // Material
    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource_->GetGPUVirtualAddress()
    );

    // ---------------------------------
    // 既存グループ描画
    // ---------------------------------

    uint32_t numInstance =
        particleSystem_.GetNumInstance();

    if (numInstance > 0) {

        commandList->SetGraphicsRootDescriptorTable(
            1,
            instancingSrvHandleGPU_
        );

        commandList->SetGraphicsRootDescriptorTable(
            2,
            textureSrvHandleGPU_
        );

        commandList->DrawInstanced(
            vertexCount_,
            numInstance,
            0,
            0
        );
    }

    // ---------------------------------
    // 追加グループ描画
    // ---------------------------------

    for (auto& [name, group] : particleGroups_) {

        uint32_t groupInstance =
            group->particleSystem.GetNumInstance();

        if (groupInstance == 0) {
            continue;
        }

        // グループ専用StructuredBuffer
        commandList->SetGraphicsRootDescriptorTable(
            1,
            group->instancingSrvHandleGPU
        );

        // グループ専用テクスチャ
        commandList->SetGraphicsRootDescriptorTable(
            2,
            group->textureSrvHandleGPU
        );

        // グループのParticleを描画
        commandList->DrawInstanced(
            vertexCount_,
            groupInstance,
            0,
            0
        );
    }
}

// =====================================
// 終了処理
// =====================================
void ParticleManager::Finalize() {

    // ---------------------------------
    // 追加グループのGPUリソースを解放
    // ---------------------------------
    for (auto& [name, group] : particleGroups_) {

        if (group->instancingResource != nullptr &&
            group->instancingData != nullptr) {

            group->instancingResource->Unmap(
                0,
                nullptr
            );

            group->instancingData = nullptr;
        }

        group->instancingResource.Reset();
    }

    // 追加グループを削除
    particleGroups_.clear();

    hasAccelerationField_ = false;

    if (instancingResource_ != nullptr &&
        instancingData_ != nullptr) {

        instancingResource_->Unmap(
            0,
            nullptr
        );

        instancingData_ = nullptr;
    }

    if (materialResource_ != nullptr &&
        materialData_ != nullptr) {

        materialResource_->Unmap(
            0,
            nullptr
        );

        materialData_ = nullptr;
    }

    // GPUリソース解放
    instancingResource_.Reset();
    materialResource_.Reset();
    vertexResource_.Reset();

    pipelineState_.Reset();
    rootSignature_.Reset();

    vertexBufferView_ = {};
    vertexCount_ = 0;

    instancingSrvHandleGPU_ = {};
    textureSrvHandleGPU_ = {};

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

// =====================================
// パーティクルグループ作成
// =====================================
void ParticleManager::CreateParticleGroup(
    const std::string& groupName,
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
) {
    assert(dxCommon_ != nullptr);
    assert(srvManager_ != nullptr);
    assert(!groupName.empty());
    assert(textureHandle.ptr != 0);

    // 同じ名前のグループがないか確認
    assert(
        particleGroups_.find(groupName) ==
        particleGroups_.end()
    );

    assert(srvManager_->CanAllocate());

    // 新しいグループ
    auto group =
        std::make_unique<ParticleGroup>();

    // ParticleSystem初期化
    group->particleSystem.Initialize();

    // AccelerationFieldを反映
    if (hasAccelerationField_) {
        group->particleSystem.SetAccelerationField(
            accelerationField_
        );
    }

    const uint32_t maxInstance =
        ParticleSystem::kNumMaxInstance;

    // インスタンシング用リソース
    group->instancingResource =
        dxCommon_->CreateBufferResource(
            sizeof(ParticleForGPU) * maxInstance
        );

    assert(group->instancingResource != nullptr);

    // CPUから書き込めるようにMap
    HRESULT hr =
        group->instancingResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(
                &group->instancingData
                )
        );

    assert(SUCCEEDED(hr));

    // 初期化
    for (uint32_t i = 0; i < maxInstance; ++i) {
        group->instancingData[i].WVP =
            MakeIdentity4x4();

        group->instancingData[i].World =
            MakeIdentity4x4();

        group->instancingData[i].color = {
            1.0f, 1.0f, 1.0f, 1.0f
        };

        group->instancingData[i].dissolveThreshold =
            0.0f;
    }

    // グループ専用のSRV番号を割り当てる
    group->srvIndex =
        srvManager_->Allocate();

    // StructuredBuffer用SRVを作成
    srvManager_->CreateSRVForStructuredBuffer(
        group->srvIndex,
        group->instancingResource.Get(),
        maxInstance,
        sizeof(ParticleForGPU)
    );

    // GPUハンドルを保存
    group->instancingSrvHandleGPU =
        srvManager_->GetGPUDescriptorHandle(
            group->srvIndex
        );

    // テクスチャを設定
    group->textureSrvHandleGPU =
        textureHandle;

    // グループ登録
    particleGroups_.emplace(
        groupName,
        std::move(group)
    );
}

// =====================================
// グループを指定してParticle発生
// =====================================
void ParticleManager::Emit(
    const std::string& groupName,
    const Emitter& emitter
) {
    auto it = particleGroups_.find(
        groupName
    );

    assert(it != particleGroups_.end());

    it->second->particleSystem.Emit(
        emitter
    );
}