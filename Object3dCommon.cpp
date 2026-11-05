#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "Logger.h"

#include <cassert>
#include <cstdint>

// =====================================
// 初期化
// =====================================
void Object3dCommon::Initialize(
    DirectXCommon* dxCommon
) {
    assert(dxCommon != nullptr);

    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreateGraphicsPipeline();
}

// =====================================
// RootSignature生成
// =====================================
void Object3dCommon::CreateRootSignature() {

    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device != nullptr);

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ---------------------------------
    // RootParameter
    // ---------------------------------

    D3D12_ROOT_PARAMETER rootParameters[7]{};

    // Material b0 (PS)
    rootParameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // WVP b0 (VS)
    rootParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // Texture t0 (PS)
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.RangeType =
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges =
        &textureRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // DirectionalLight b1 (PS)
    rootParameters[3].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // Camera b2 (PS)
    rootParameters[4].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 2;

    // PointLight b3 (PS)
    rootParameters[5].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 3;

    // SpotLight b4 (PS)
    rootParameters[6].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 4;

    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    // ---------------------------------
    // Sampler
    // ---------------------------------

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
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    desc.pStaticSamplers = &sampler;
    desc.NumStaticSamplers = 1;

    // ---------------------------------
    // シリアライズ
    // ---------------------------------

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

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
        IID_PPV_ARGS(rootSignature_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// PSO生成
// =====================================
void Object3dCommon::CreateGraphicsPipeline() {

    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device != nullptr);

    // ---------------------------------
    // シェーダー
    // ---------------------------------

    auto vertexShader = dxCommon_->CompileShader(
        L"Object3d.VS.hlsl",
        L"vs_6_0"
    );

    auto pixelShader = dxCommon_->CompileShader(
        L"Object3d.PS.hlsl",
        L"ps_6_0"
    );

    assert(vertexShader != nullptr);
    assert(pixelShader != nullptr);

    // ---------------------------------
    // InputLayout
    // ---------------------------------

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

    // ---------------------------------
    // Rasterizer
    // ---------------------------------

    D3D12_RASTERIZER_DESC rasterizer{};
    rasterizer.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.DepthClipEnable = TRUE;

    // ---------------------------------
    // DepthStencil
    // ---------------------------------

    D3D12_DEPTH_STENCIL_DESC depthStencil{};
    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask =
        D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc =
        D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ---------------------------------
    // PSO共通設定
    // ---------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

    psoDesc.pRootSignature = rootSignature_.Get();
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

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat =
        DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // ---------------------------------
    // ブレンドモード別のPSO
    // ---------------------------------

    for (uint32_t i = 0; i < kCountOfBlendMode; ++i) {

        D3D12_BLEND_DESC blend{};
        D3D12_RENDER_TARGET_BLEND_DESC& target =
            blend.RenderTarget[0];

        target.RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;

        target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOpAlpha = D3D12_BLEND_OP_ADD;

        target.BlendEnable =
            (i == kBlendModeNone) ? FALSE : TRUE;

        target.BlendOp = D3D12_BLEND_OP_ADD;

        switch (i) {
        case kBlendModeNone:
            break;

        case kBlendModeNormal:
            target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            target.DestBlend =
                D3D12_BLEND_INV_SRC_ALPHA;
            break;

        case kBlendModeAdd:
            target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            target.DestBlend = D3D12_BLEND_ONE;
            break;

        case kBlendModeSub:
            target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            target.DestBlend = D3D12_BLEND_ONE;
            target.BlendOp =
                D3D12_BLEND_OP_REV_SUBTRACT;
            break;

        case kBlendModeMul:
            target.SrcBlend = D3D12_BLEND_ZERO;
            target.DestBlend = D3D12_BLEND_SRC_COLOR;
            break;

        case kBlendModeScreen:
            target.SrcBlend =
                D3D12_BLEND_INV_DEST_COLOR;
            target.DestBlend = D3D12_BLEND_ONE;
            break;

        default:
            assert(false);
            break;
        }

        psoDesc.BlendState = blend;

        HRESULT hr = device->CreateGraphicsPipelineState(
            &psoDesc,
            IID_PPV_ARGS(
                graphicsPipelineStates_[i].GetAddressOf()
            )
        );

        assert(SUCCEEDED(hr));
    }
}

// =====================================
// 3D共通描画設定
// =====================================
void Object3dCommon::CommonDraw(
    BlendMode blendMode
) {
    assert(dxCommon_ != nullptr);

    assert(
        blendMode >= kBlendModeNone &&
        blendMode < kCountOfBlendMode
    );

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();

    assert(commandList != nullptr);

    commandList->SetGraphicsRootSignature(
        rootSignature_.Get()
    );

    commandList->SetPipelineState(
        graphicsPipelineStates_[blendMode].Get()
    );

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );
}

// =====================================
// 終了処理
// =====================================
void Object3dCommon::Finalize() {

    // PSOを解放
    for (auto& pipelineState : graphicsPipelineStates_) {
        pipelineState.Reset();
    }

    // RootSignatureを解放
    rootSignature_.Reset();

    // DirectXCommonへの参照解除
    dxCommon_ = nullptr;
}