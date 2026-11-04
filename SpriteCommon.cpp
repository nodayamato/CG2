#include "SpriteCommon.h"
#include "DirectXCommon.h"

#include <cassert>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

// =====================================
// 初期化
// =====================================
void SpriteCommon::Initialize(DirectXCommon* dxCommon) {

    assert(dxCommon != nullptr);

    dxCommon_ = dxCommon;

    // RootSignatureを作成
    CreateRootSignature();

    // PSOを作成
    CreateGraphicsPipeline();
}

// =====================================
// RootSignature作成
// =====================================
void SpriteCommon::CreateRootSignature() {

    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device != nullptr);

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ---------------------------------
    // RootParameter
    // ---------------------------------

    D3D12_ROOT_PARAMETER rootParameters[3]{};

    // 0: Material用CBV (PixelShader b0)
    rootParameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;

    rootParameters[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[0].Descriptor.ShaderRegister = 0;

    // 1: WVP用CBV (VertexShader b0)
    rootParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;

    rootParameters[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    rootParameters[1].Descriptor.ShaderRegister = 0;

    // 2: Texture用SRV (PixelShader t0)
    D3D12_DESCRIPTOR_RANGE descriptorRange{};

    descriptorRange.RangeType =
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0;

    descriptorRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

    rootParameters[2].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[2].DescriptorTable.pDescriptorRanges =
        &descriptorRange;

    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    // ---------------------------------
    // Sampler
    // ---------------------------------

    D3D12_STATIC_SAMPLER_DESC staticSampler{};

    staticSampler.Filter =
        D3D12_FILTER_MIN_MAG_MIP_LINEAR;

    staticSampler.AddressU =
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    staticSampler.AddressV =
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    staticSampler.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    staticSampler.ComparisonFunc =
        D3D12_COMPARISON_FUNC_NEVER;

    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;

    staticSampler.ShaderRegister = 0;

    staticSampler.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    // ---------------------------------
    // シリアライズ
    // ---------------------------------

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    assert(SUCCEEDED(hr));

    // ---------------------------------
    // RootSignature生成
    // ---------------------------------

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// GraphicsPipeline作成
// =====================================
void SpriteCommon::CreateGraphicsPipeline() {

    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device != nullptr);

    // ---------------------------------
    // シェーダーコンパイル
    // ---------------------------------

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(
            L"Sprite.VS.hlsl",
            L"vs_6_0"
        );

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(
            L"Sprite.PS.hlsl",
            L"ps_6_0"
        );

    assert(vertexShaderBlob != nullptr);
    assert(pixelShaderBlob != nullptr);

    // ---------------------------------
    // InputLayout
    // ---------------------------------

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};

    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].Format =
        DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].Format =
        DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].Format =
        DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset =
        D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};

    inputLayoutDesc.pInputElementDescs =
        inputElementDescs;

    inputLayoutDesc.NumElements =
        _countof(inputElementDescs);

    // ---------------------------------
    // BlendState
    // ---------------------------------

    D3D12_BLEND_DESC blendDesc{};

    blendDesc.RenderTarget[0].BlendEnable = TRUE;

    blendDesc.RenderTarget[0].SrcBlend =
        D3D12_BLEND_SRC_ALPHA;

    blendDesc.RenderTarget[0].DestBlend =
        D3D12_BLEND_INV_SRC_ALPHA;

    blendDesc.RenderTarget[0].BlendOp =
        D3D12_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].SrcBlendAlpha =
        D3D12_BLEND_ONE;

    blendDesc.RenderTarget[0].DestBlendAlpha =
        D3D12_BLEND_ZERO;

    blendDesc.RenderTarget[0].BlendOpAlpha =
        D3D12_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // ---------------------------------
    // RasterizerState
    // ---------------------------------

    D3D12_RASTERIZER_DESC rasterizerDesc{};

    // スプライトは両面描画
    rasterizerDesc.CullMode =
        D3D12_CULL_MODE_NONE;

    rasterizerDesc.FillMode =
        D3D12_FILL_MODE_SOLID;

    rasterizerDesc.DepthClipEnable = TRUE;

    // ---------------------------------
    // DepthStencilState
    // ---------------------------------

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

    // 2Dスプライトでは深度テストを使用しない
    depthStencilDesc.DepthEnable = FALSE;

    depthStencilDesc.DepthWriteMask =
        D3D12_DEPTH_WRITE_MASK_ZERO;

    depthStencilDesc.DepthFunc =
        D3D12_COMPARISON_FUNC_ALWAYS;

    // ---------------------------------
    // PipelineState
    // ---------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC
        graphicsPipelineStateDesc{};

    graphicsPipelineStateDesc.pRootSignature =
        rootSignature_.Get();

    graphicsPipelineStateDesc.InputLayout =
        inputLayoutDesc;

    graphicsPipelineStateDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };

    graphicsPipelineStateDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };

    graphicsPipelineStateDesc.BlendState = blendDesc;

    graphicsPipelineStateDesc.RasterizerState =
        rasterizerDesc;

    graphicsPipelineStateDesc.DepthStencilState =
        depthStencilDesc;

    graphicsPipelineStateDesc.NumRenderTargets = 1;

    graphicsPipelineStateDesc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    graphicsPipelineStateDesc.DSVFormat =
        DXGI_FORMAT_D24_UNORM_S8_UINT;

    graphicsPipelineStateDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    graphicsPipelineStateDesc.SampleDesc.Count = 1;

    graphicsPipelineStateDesc.SampleMask =
        D3D12_DEFAULT_SAMPLE_MASK;

    // ---------------------------------
    // PSO生成
    // ---------------------------------

    HRESULT hr = device->CreateGraphicsPipelineState(
        &graphicsPipelineStateDesc,
        IID_PPV_ARGS(graphicsPipelineState_.GetAddressOf())
    );

    assert(SUCCEEDED(hr));
}

// =====================================
// 共通描画処理
// =====================================
void SpriteCommon::CommonDraw() {

    assert(dxCommon_ != nullptr);

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();

    assert(commandList != nullptr);

    // RootSignature
    commandList->SetGraphicsRootSignature(
        rootSignature_.Get()
    );

    // PSO
    commandList->SetPipelineState(
        graphicsPipelineState_.Get()
    );

    // 三角形で描画
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );
}