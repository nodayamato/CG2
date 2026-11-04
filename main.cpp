#pragma warning(push)
// C4023の警告を無効化する
#pragma warning(disable:4023)

#include <Windows.h>

// 自作ヘッダー
#include "Math.h"
#include "ModelData.h"
#include "Audio.h"
#include "Input.h"
#include "DebugCamera.h"
#include "BlendMode.h"
#include "ParticleSystem.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "D3DResourceLeakChecker.h"
#include <wrl.h>

// 標準ライブラリ
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <vector>
#include <numbers>
#include <format>
#include <ostream>

// DirectX12関連
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>

// DirectInput
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma warning(pop)

// ライブラリのリンク設定
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

#endif

/// <summary>
/// クラッシュダンプを出力する
/// </summary>
/// <param name="exception"></param>
/// <returns></returns>
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);

	wchar_t filePath[MAX_PATH] = { 0 };

	CreateDirectory(L"./Dumps", nullptr);

	StringCchPrintf(
		filePath,
		MAX_PATH,
		L"./Dumps/%04d-%02d%02d-%02d%02d%02d.dmp",
		time.wYear,
		time.wMonth,
		time.wDay,
		time.wHour,
		time.wMinute,
		time.wSecond
	);

	HANDLE dumpFileHandle = CreateFile(
		filePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		0,
		CREATE_ALWAYS,
		0,
		0
	);

	// processId（このexeのID）とクラッシュ（例外）の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dumpを出力
	MiniDumpWriteDump(
		GetCurrentProcess(),
		processId,
		dumpFileHandle,
		MiniDumpNormal,
		&minidumpInformation,
		nullptr,
		nullptr
	);

	CloseHandle(dumpFileHandle);

	return EXCEPTION_EXECUTE_HANDLER;
}

// Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// COMの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// リソースリークチェック
	D3DResourceLeakChecker leakChecker;

	// Dump出力を設定
	SetUnhandledExceptionFilter(ExportDump);

	// 音声管理
	Audio audio;

	// 入力管理
	Input* input = new Input();

	// デバッグカメラ
	DebugCamera debugCamera;

	// デバッグカメラ初期化
	debugCamera.Initialize();

	// ------------------------------
	// ウィンドウ関連の初期化
	// ------------------------------

	// WinAppクラスのインスタンスを生成
	WinApp winApp;
	// ウィンドウ初期化
	winApp.Initialize();

#ifdef _DEBUG
	// デバッグレイヤー
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(
		IID_PPV_ARGS(debugController.GetAddressOf())
	))) {

		// デバッグレイヤー有効化
		debugController->EnableDebugLayer();

		// GPU側の検証も有効化
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif

	// ---------------------------------
	// ログ関連の初期化
	// ---------------------------------

	// Loggerの初期化
	Logger::Initialize();

	// ---------------------------------
	// DirectX12基盤の初期化
	// ---------------------------------

	DirectXCommon dxCommon;
	// WinAppを渡して初期化
	dxCommon.Initialize(&winApp);

	// DirectXCommonが所有するデバイスを取得
	ID3D12Device* device = dxCommon.GetDevice();

	// 後続のDirectX12処理で使用
	HRESULT hr = S_OK;

	// 入力の初期化
	input->Initialize(winApp.GetHInstance(), winApp.GetHwnd());

#ifdef _DEBUG
	// デバッグレイヤーの情報を取得する
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {

		// ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(
			D3D12_MESSAGE_SEVERITY_CORRUPTION,
			true
		);

		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(
			D3D12_MESSAGE_SEVERITY_ERROR,
			true
		);

		// 警告が出たときにコメントアウトすると場所がわかる！
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(
			D3D12_MESSAGE_SEVERITY_WARNING,
			true
		);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {

			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};

		D3D12_INFO_QUEUE_FILTER filter{};

		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;

		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
		// 解放
		infoQueue.Reset();
	}

#endif

	// ------------------------------------
	// DirectXCommonが管理するリソースを取得
	// ------------------------------------

	// コマンドリスト
	ID3D12GraphicsCommandList* commandList = dxCommon.GetCommandList();

	// --------------------------------------
	// XAudio2初期化
	// --------------------------------------

	// XAudio2の初期化
	audio.Initialize();

	// WAVファイルを読み込む
	audio.LoadWave("resources/Alarm01.wav");

	// 音声を再生する
	audio.PlayWave();

	// ---------------------------------
	// Object3D用シェーダー
	// ---------------------------------

	// VertexShader
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		dxCommon.CompileShader(
			L"Object3d.VS.hlsl",
			L"vs_6_0"
		);

	assert(vertexShaderBlob != nullptr);

	// PixelShader
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		dxCommon.CompileShader(
			L"Object3d.PS.hlsl",
			L"ps_6_0"
		);

	assert(pixelShaderBlob != nullptr);

	// ---------------------------------
	// Particle用シェーダー
	// ---------------------------------

	// Particle VertexShader
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlob =
		dxCommon.CompileShader(
			L"Particle.VS.hlsl",
			L"vs_6_0"
		);

	assert(particleVertexShaderBlob != nullptr);

	// Particle PixelShader
	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlob =
		dxCommon.CompileShader(
			L"Particle.PS.hlsl",
			L"ps_6_0"
		);

	assert(particlePixelShaderBlob != nullptr);

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[7] = {};
	// CBVを設定
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	// PixelShaderを設定
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// b0を設定
	rootParameters[0].Descriptor.ShaderRegister = 0;
	// CBVを設定
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	// VertexShaderを設定
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// b0を設定
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// Sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
	// RootSignatureへ設定
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	// SRV用DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Texture(SRV)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	// 平行光源用のCBVを設定
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	// PixelShaderで使用する
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// HLSL側のregister(b1)と対応
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// Camera用CBV
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// HLSLの register(b2)
	rootParameters[4].Descriptor.ShaderRegister = 2;

	// PointLight用CBV
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// HLSLの register(b3)
	rootParameters[5].Descriptor.ShaderRegister = 3;

	// SpotLight用CBV
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// HLSL側の register(b4)
	rootParameters[6].Descriptor.ShaderRegister = 4;

	// ------------------------------------
	// Object3D用RootSignature
	// ------------------------------------

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (FAILED(hr)) {
		if (errorBlob != nullptr) {
			Logger::Log(reinterpret_cast<const char*>(
				errorBlob->GetBufferPointer()
				));
		}
		assert(false);
	}

	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureOwner;

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignatureOwner.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	// 既存の描画処理で使うポインタ
	ID3D12RootSignature* rootSignature = rootSignatureOwner.Get();

	// -----------------
	// Particle用RootSignature
	// -----------------

	D3D12_ROOT_SIGNATURE_DESC particleRootSignatureDesc{};
	particleRootSignatureDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// -----------------
	// DescriptorRange
	// -----------------

	// Instancing用 SRV
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Texture用 SRV
	D3D12_DESCRIPTOR_RANGE particleTextureDescriptorRange[1] = {};
	particleTextureDescriptorRange[0].BaseShaderRegister = 0;
	particleTextureDescriptorRange[0].NumDescriptors = 1;
	particleTextureDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleTextureDescriptorRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// -----------------
	// RootParameter
	// -----------------

	D3D12_ROOT_PARAMETER particleRootParameters[3] = {};

	// Material b0
	particleRootParameters[0].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV;
	particleRootParameters[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[0].Descriptor.ShaderRegister = 0;

	// Instancing t0
	particleRootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[1].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_VERTEX;
	particleRootParameters[1].DescriptorTable.pDescriptorRanges =
		descriptorRangeForInstancing;
	particleRootParameters[1].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRangeForInstancing);

	// Texture t0
	particleRootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[2].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[2].DescriptorTable.pDescriptorRanges =
		particleTextureDescriptorRange;
	particleRootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(particleTextureDescriptorRange);

	// -----------------
	// Sampler
	// -----------------

	D3D12_STATIC_SAMPLER_DESC particleStaticSamplers[1] = {};

	particleStaticSamplers[0].Filter =
		D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	particleStaticSamplers[0].AddressU =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleStaticSamplers[0].AddressV =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleStaticSamplers[0].AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleStaticSamplers[0].ComparisonFunc =
		D3D12_COMPARISON_FUNC_NEVER;
	particleStaticSamplers[0].MaxLOD =
		D3D12_FLOAT32_MAX;
	particleStaticSamplers[0].ShaderRegister = 0;
	particleStaticSamplers[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;

	particleRootSignatureDesc.pParameters =
		particleRootParameters;
	particleRootSignatureDesc.NumParameters =
		_countof(particleRootParameters);

	particleRootSignatureDesc.pStaticSamplers =
		particleStaticSamplers;
	particleRootSignatureDesc.NumStaticSamplers =
		_countof(particleStaticSamplers);

	// ------------------------------------
	// Particle用RootSignature
	// ------------------------------------

	Microsoft::WRL::ComPtr<ID3DBlob>
		particleSignatureBlob;

	Microsoft::WRL::ComPtr<ID3DBlob>
		particleErrorBlob;

	hr = D3D12SerializeRootSignature(
		&particleRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		particleSignatureBlob.GetAddressOf(),
		particleErrorBlob.GetAddressOf()
	);

	if (FAILED(hr)) {
		if (particleErrorBlob != nullptr) {
			Logger::Log(reinterpret_cast<const char*>(
				particleErrorBlob->GetBufferPointer()
				));
		}
		assert(false);
	}

	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignatureOwner;

	hr = device->CreateRootSignature(
		0,
		particleSignatureBlob->GetBufferPointer(),
		particleSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(
			particleRootSignatureOwner.GetAddressOf()
		)
	);

	assert(SUCCEEDED(hr));

	// 既存の描画処理で使うポインタ
	ID3D12RootSignature* particleRootSignature = particleRootSignatureOwner.Get();

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	// 頂点座標
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// テクスチャ座標
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// 法線
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// -----------------
	// BlendState
	// -----------------

	D3D12_BLEND_DESC blendDesc[kCountOfBlendMode]{};

	for (uint32_t i = 0; i < kCountOfBlendMode; ++i) {
		blendDesc[i].RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		blendDesc[i].RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc[i].RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc[i].RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	}

	// ブレンドなし
	blendDesc[kBlendModeNone].RenderTarget[0].BlendEnable = FALSE;

	// 通常ブレンド
	blendDesc[kBlendModeNormal].RenderTarget[0].BlendEnable = TRUE;
	blendDesc[kBlendModeNormal].RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc[kBlendModeNormal].RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc[kBlendModeNormal].RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

	// 加算ブレンド
	blendDesc[kBlendModeAdd].RenderTarget[0].BlendEnable = TRUE;
	blendDesc[kBlendModeAdd].RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc[kBlendModeAdd].RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc[kBlendModeAdd].RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	// 減算ブレンド
	blendDesc[kBlendModeSub].RenderTarget[0].BlendEnable = TRUE;
	blendDesc[kBlendModeSub].RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc[kBlendModeSub].RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	blendDesc[kBlendModeSub].RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	// 乗算ブレンド
	blendDesc[kBlendModeMul].RenderTarget[0].BlendEnable = TRUE;
	blendDesc[kBlendModeMul].RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
	blendDesc[kBlendModeMul].RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc[kBlendModeMul].RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;

	// スクリーンブレンド
	blendDesc[kBlendModeScreen].RenderTarget[0].BlendEnable = TRUE;
	blendDesc[kBlendModeScreen].RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	blendDesc[kBlendModeScreen].RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc[kBlendModeScreen].RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// 裏面カリング
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// 塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	// Depthの機能を有効にする
	depthStencilDesc.DepthEnable = true;

	// 深度値を書き込む
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	// 比較関数はLessEqual
	// すでに書き込まれている深度値と同じか、より手前なら描画する
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	graphicsPipelineStateDesc.pRootSignature = rootSignature;

	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};

	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};

	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// DepthStencilStateを設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	// DSVのFormatを設定
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTV情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// 利用するトポロジ
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプル数
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// -----------------
	// ブレンドモードごとのPSOを生成
	// -----------------

	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStatesOwner[kCountOfBlendMode];

	// 描画処理で使う通常ポインタ
	ID3D12PipelineState* graphicsPipelineStates[kCountOfBlendMode] = {};

	for (uint32_t i = 0; i < kCountOfBlendMode; ++i) {

		graphicsPipelineStateDesc.BlendState = blendDesc[i];

		hr = device->CreateGraphicsPipelineState(
			&graphicsPipelineStateDesc,
			IID_PPV_ARGS(
				graphicsPipelineStatesOwner[i].GetAddressOf()
			)
		);

		assert(SUCCEEDED(hr));

		graphicsPipelineStates[i] = graphicsPipelineStatesOwner[i].Get();
	}

	// -----------------
	// Particle用PSO
	// -----------------

	D3D12_GRAPHICS_PIPELINE_STATE_DESC
		particlePipelineStateDesc =
		graphicsPipelineStateDesc;

	// Particleは両面描画
	particlePipelineStateDesc.RasterizerState.CullMode =
		D3D12_CULL_MODE_NONE;

	particlePipelineStateDesc.DepthStencilState.DepthWriteMask =
		D3D12_DEPTH_WRITE_MASK_ZERO;

	particlePipelineStateDesc.pRootSignature =
		particleRootSignature;

	particlePipelineStateDesc.VS = {
		particleVertexShaderBlob->GetBufferPointer(),
		particleVertexShaderBlob->GetBufferSize()
	};

	particlePipelineStateDesc.PS = {
		particlePixelShaderBlob->GetBufferPointer(),
		particlePixelShaderBlob->GetBufferSize()
	};

	// ParticleはNormalブレンド
	particlePipelineStateDesc.BlendState = blendDesc[kBlendModeNormal];

	Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePipelineStateOwner;

	hr = device->CreateGraphicsPipelineState(
		&particlePipelineStateDesc,
		IID_PPV_ARGS(
			particlePipelineStateOwner.GetAddressOf()
		)
	);

	assert(SUCCEEDED(hr));

	// 描画処理で使う通常ポインタ
	ID3D12PipelineState* particlePipelineState = particlePipelineStateOwner.Get();

	// 現在のブレンドモード
	BlendMode blendMode = kBlendModeNormal;

	// ------------------------------
	// Particle用のModelData
	// ------------------------------

	ModelData particleModelData;

	// 左上
	particleModelData.vertices.push_back({
		.position = {-1.0f, 1.0f, 0.0f, 1.0f},
		.texcoord = {0.0f, 0.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// 右上
	particleModelData.vertices.push_back({
		.position = {1.0f, 1.0f, 0.0f, 1.0f},
		.texcoord = {1.0f, 0.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// 左下
	particleModelData.vertices.push_back({
		.position = {-1.0f, -1.0f, 0.0f, 1.0f},
		.texcoord = {0.0f, 1.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// 左下
	particleModelData.vertices.push_back({
		.position = {-1.0f, -1.0f, 0.0f, 1.0f},
		.texcoord = {0.0f, 1.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// 右上
	particleModelData.vertices.push_back({
		.position = {1.0f, 1.0f, 0.0f, 1.0f},
		.texcoord = {1.0f, 0.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// 右下
	particleModelData.vertices.push_back({
		.position = {1.0f, -1.0f, 0.0f, 1.0f},
		.texcoord = {1.0f, 1.0f},
		.normal = {0.0f, 0.0f, -1.0f}
		});

	// -----------------
	// Particle用VertexResource
	// -----------------

	auto particleVertexResourceOwner =
		dxCommon.CreateBufferResource(
			sizeof(VertexData) * particleModelData.vertices.size()
		);

	ID3D12Resource* particleVertexResource =particleVertexResourceOwner.Get();

	// -----------------
	// Particle用VertexBufferView
	// -----------------

	D3D12_VERTEX_BUFFER_VIEW particleVertexBufferView{};

	particleVertexBufferView.BufferLocation =
		particleVertexResource->GetGPUVirtualAddress();

	particleVertexBufferView.SizeInBytes =
		static_cast<UINT>(
			sizeof(VertexData) * particleModelData.vertices.size()
			);

	particleVertexBufferView.StrideInBytes =
		sizeof(VertexData);

	// -----------------
	// Particle頂点データ
	// -----------------

	VertexData* particleVertexData = nullptr;

	HRESULT particleVertexMapResult =
		particleVertexResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&particleVertexData)
		);

	assert(SUCCEEDED(particleVertexMapResult));

	std::memcpy(
		particleVertexData,
		particleModelData.vertices.data(),
		sizeof(VertexData) * particleModelData.vertices.size()
	);

	// -----------------
	// Instancing
	// -----------------

	// Instancing用Resource
	auto instancingResourceOwner =
		dxCommon.CreateBufferResource(
			sizeof(ParticleForGPU) * ParticleSystem::kNumMaxInstance
		);

	ID3D12Resource* instancingResource = instancingResourceOwner.Get();

	// 書き込み先
	ParticleForGPU* instancingData = nullptr;

	instancingResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&instancingData)
	);

	// 初期化
	for (uint32_t index = 0;
		index < ParticleSystem::kNumMaxInstance;
		++index) {

		instancingData[index].WVP =
			MakeIdentity4x4();

		instancingData[index].World =
			MakeIdentity4x4();

		instancingData[index].color = {
			1.0f, 1.0f, 1.0f, 1.0f
		};

		instancingData[index].dissolveThreshold = 0.0f;
	}

	// -----------------
	// ParticleSystem
	// -----------------
	ParticleSystem particleSystem;

	particleSystem.Initialize();

	// -----------------
	// AccelerationField
	// -----------------
	AccelerationField accelerationField{};

	accelerationField.acceleration = {
		15.0f,
		0.0f,
		0.0f
	};

	accelerationField.area.min = {
		-1.0f,
		-1.0f,
		-1.0f
	};

	accelerationField.area.max = {
		1.0f,
		1.0f,
		1.0f
	};

	particleSystem.SetAccelerationField(
		accelerationField
	);

	// -----------------
	// Emitter
	// -----------------
	Emitter emitter{};

	emitter.transform.scale = {
		1.0f, 1.0f, 1.0f
	};

	emitter.transform.rotate = {
		0.0f, 0.0f, 0.0f
	};

	emitter.transform.translate = {
		0.0f, 0.0f, 0.0f
	};

	emitter.count = 3;
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	// Billboardを使うか
	bool useBillboard = true;

	// AccelerationFieldを使うか
	bool useAccelerationField = true;

	// Particle用UVTransform
	Transform particleUVTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	// 1フレームの時間
	const float kDeltaTime = 1.0f / 60.0f;

	// ------------------------------
	// 球モデルを生成
	// ------------------------------

	ModelData modelData{};

	// -----------------
	// 球の分割数
	// -----------------
	const uint32_t kSubdivision = 16;

	// -----------------
	// 経度1つ分の角度
	// -----------------
	const float kLonEvery =
		std::numbers::pi_v<float> *2.0f /
		static_cast<float>(kSubdivision);

	// -----------------
	// 緯度1つ分の角度
	// -----------------
	const float kLatEvery =
		std::numbers::pi_v<float> /
		static_cast<float>(kSubdivision);

	// -----------------
	// 頂点数を確保
	// 1マスにつき三角形2枚 = 6頂点
	// -----------------
	modelData.vertices.resize(
		kSubdivision *
		kSubdivision *
		6
	);

	// -----------------
	// 緯度方向
	// -----------------
	for (uint32_t latIndex = 0;
		latIndex < kSubdivision;
		++latIndex) {

		// -π/2 ～ π/2
		float lat =
			-std::numbers::pi_v<float> / 2.0f +
			kLatEvery *
			static_cast<float>(latIndex);

		float nextLat =
			lat +
			kLatEvery;

		// -----------------
		// 経度方向
		// -----------------
		for (uint32_t lonIndex = 0;
			lonIndex < kSubdivision;
			++lonIndex) {

			float lon =
				kLonEvery *
				static_cast<float>(lonIndex);

			float nextLon =
				lon +
				kLonEvery;

			// -----------------
			// このマスの先頭Index
			// -----------------
			uint32_t start =
				(latIndex * kSubdivision +
					lonIndex) *
				6;

			// -----------------
			// a : 左下
			// -----------------
			Vector4 a = {
				std::cos(lat) *
					std::cos(lon),

				std::sin(lat),

				std::cos(lat) *
					std::sin(lon),

				1.0f
			};

			// -----------------
			// b : 左上
			// -----------------
			Vector4 b = {
				std::cos(nextLat) *
					std::cos(lon),

				std::sin(nextLat),

				std::cos(nextLat) *
					std::sin(lon),

				1.0f
			};

			// -----------------
			// c : 右下
			// -----------------
			Vector4 c = {
				std::cos(lat) *
					std::cos(nextLon),

				std::sin(lat),

				std::cos(lat) *
					std::sin(nextLon),

				1.0f
			};

			// -----------------
			// d : 右上
			// -----------------
			Vector4 d = {
				std::cos(nextLat) *
					std::cos(nextLon),

				std::sin(nextLat),

				std::cos(nextLat) *
					std::sin(nextLon),

				1.0f
			};

			// -----------------
			// UV
			// -----------------

			float u0 =
				static_cast<float>(lonIndex) /
				static_cast<float>(kSubdivision);

			float u1 =
				static_cast<float>(lonIndex + 1) /
				static_cast<float>(kSubdivision);

			float v0 =
				1.0f -
				static_cast<float>(latIndex) /
				static_cast<float>(kSubdivision);

			float v1 =
				1.0f -
				static_cast<float>(latIndex + 1) /
				static_cast<float>(kSubdivision);

			// -----------------
			// 1枚目
			// a → b → c
			// -----------------

			modelData.vertices[start + 0] = {
				.position = a,
				.texcoord = {u0, v0},
				.normal = {a.x, a.y, a.z}
			};

			modelData.vertices[start + 1] = {
				.position = b,
				.texcoord = {u0, v1},
				.normal = {b.x, b.y, b.z}
			};

			modelData.vertices[start + 2] = {
				.position = c,
				.texcoord = {u1, v0},
				.normal = {c.x, c.y, c.z}
			};

			// -----------------
			// 2枚目
			// c → b → d
			// -----------------

			modelData.vertices[start + 3] = {
				.position = c,
				.texcoord = {u1, v0},
				.normal = {c.x, c.y, c.z}
			};

			modelData.vertices[start + 4] = {
				.position = b,
				.texcoord = {u0, v1},
				.normal = {b.x, b.y, b.z}
			};

			modelData.vertices[start + 5] = {
				.position = d,
				.texcoord = {u1, v1},
				.normal = {d.x, d.y, d.z}
			};
		}
	}

	// 頂点が作れているか確認
	assert(!modelData.vertices.empty());

	// ------------------------------
	// モデル用頂点リソースを作成
	// ------------------------------

	// 読み込んだモデルの頂点数に合わせて頂点バッファを作る
	auto vertexResourceOwner =
		dxCommon.CreateBufferResource(
			sizeof(VertexData) * modelData.vertices.size()
		);

	ID3D12Resource* vertexResource =vertexResourceOwner.Get();

	// モデル用頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	// 頂点リソースのGPUアドレスを設定する
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	// 頂点バッファ全体のサイズを設定する
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData.vertices.size());

	// 1頂点分のサイズを設定する
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// ------------------------------
	// モデルの頂点データを書き込む
	// ------------------------------

	// 頂点リソースの書き込み先
	VertexData* vertexData = nullptr;

	// CPUから書き込むためにMapする
	HRESULT vertexMapResult = vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	assert(SUCCEEDED(vertexMapResult));

	// LoadObjFileで読み込んだ全頂点を頂点リソースへコピーする
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// ------------------------------
	// Material用リソースを作る
	// ------------------------------

	// Material用リソース
	auto materialResourceOwner = dxCommon.CreateBufferResource(256);
	ID3D12Resource* materialResource = materialResourceOwner.Get();

	// Map
	Material* materialData = nullptr;

	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 初期値
	materialData->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData->enableLighting = true;

	// 鏡面反射の鋭さ
	materialData->shininess = 32.0f;

	// 球のUVは最初は変形しない
	materialData->uvTransform = MakeIdentity4x4();

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	auto wvpResourceOwner = dxCommon.CreateBufferResource(sizeof(TransformationMatrix));
	ID3D12Resource* wvpResource = wvpResourceOwner.Get();

	// データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	// 単位行列を書き込んでおく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();
	wvpData->WorldInverseTranspose = MakeIdentity4x4();

	// Transform変数を作る
	Transform transform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, -1.54f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	// ------------------------------
	// 2個目のモデルを読み込む
	// ------------------------------

	ModelData modelData2 =
		LoadModelFile(
			"resources/plane",
			"plane.gltf"
		);

	assert(!modelData2.vertices.empty());

	// ------------------------------
	// 2個目の頂点リソース
	// ------------------------------

	auto vertexResource2Owner =
		dxCommon.CreateBufferResource(
			sizeof(VertexData) * modelData2.vertices.size()
		);

	ID3D12Resource* vertexResource2 = vertexResource2Owner.Get();

	// ------------------------------
	// 2個目のVertexBufferView
	// ------------------------------

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView2{};

	vertexBufferView2.BufferLocation =
		vertexResource2->GetGPUVirtualAddress();

	vertexBufferView2.SizeInBytes =
		static_cast<UINT>(
			sizeof(VertexData) *
			modelData2.vertices.size()
			);

	vertexBufferView2.StrideInBytes =
		sizeof(VertexData);

	// ------------------------------
	// 頂点を書き込む
	// ------------------------------

	VertexData* vertexData2 = nullptr;

	vertexResource2->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(
			&vertexData2
			)
	);

	std::memcpy(
		vertexData2,
		modelData2.vertices.data(),
		sizeof(VertexData)*
		modelData2.vertices.size()
	);

	// ------------------------------
	// 2個目のWVP
	// ------------------------------

	auto wvpResource2Owner = dxCommon.CreateBufferResource(sizeof(TransformationMatrix));
	ID3D12Resource* wvpResource2 = wvpResource2Owner.Get();

	TransformationMatrix* wvpData2 = nullptr;

	wvpResource2->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(
			&wvpData2
			)
	);

	wvpData2->WVP =
		MakeIdentity4x4();

	wvpData2->World =
		MakeIdentity4x4();

	wvpData2->WorldInverseTranspose =
		MakeIdentity4x4();

	// ------------------------------
	// 2個目のTransform
	// ------------------------------

	Transform transform2{
		{1.0f, 1.0f, 1.0f},
		{0.0f, -1.54f, 0.0f},
		{3.0f, 0.0f, 0.0f}
	};

	// -----------------
	// Particle用Material
	// -----------------

	auto particleMaterialResourceOwner = dxCommon.CreateBufferResource(256);
	ID3D12Resource* particleMaterialResource = particleMaterialResourceOwner.Get();

	Material* particleMaterialData = nullptr;

	particleMaterialResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&particleMaterialData)
	);

	// 初期値
	particleMaterialData->color = {
		1.0f, 1.0f, 1.0f, 1.0f
	};

	particleMaterialData->enableLighting = false;
	particleMaterialData->shininess = 32.0f;
	particleMaterialData->uvTransform = MakeIdentity4x4();

	// ------------------------------
	// Camera用Resource
	// ------------------------------

	auto cameraResourceOwner = dxCommon.CreateBufferResource(256);
	ID3D12Resource* cameraResource = cameraResourceOwner.Get();

	CameraForGPU* cameraData = nullptr;

	cameraResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(
			&cameraData
			)
	);

	// Camera初期値
	cameraData->worldPosition = debugCamera.GetWorldPosition();
	cameraData->padding = 0.0f;

	// ------------------------------
	// 平行光源用リソースを作る
	// ------------------------------

	// 平行光源用ConstantBufferを作成
	auto directionalLightResourceOwner = dxCommon.CreateBufferResource(sizeof(DirectionalLight));
	ID3D12Resource* directionalLightResource = directionalLightResourceOwner.Get();
	// CPUから書き込むためのポインタ
	DirectionalLight* directionalLightData = nullptr;
	// ResourceをMapして書き込み先を取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 色
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 向き
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	// 明るさ
	directionalLightData->intensity = 1.0f;

	// ------------------------------
	// 点光源用リソースを作る
	// ------------------------------

	// 点光源用ConstantBufferを作成
	auto pointLightResourceOwner = dxCommon.CreateBufferResource(sizeof(PointLight));
	ID3D12Resource* pointLightResource = pointLightResourceOwner.Get();
	// CPUから書き込むためのポインタ
	PointLight* pointLightData = nullptr;
	// ResourceをMapして書き込み先を取得
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));
	// 色
	pointLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	// 位置
	pointLightData->position = { 0.0f,2.0f,0.0f };
	// 明るさ
	pointLightData->intensity = 1.0f;
	// ライトが届く最大距離
	pointLightData->radius = 10.0f;
	// 減衰率
	pointLightData->decay = 1.0f;
	// padding
	pointLightData->padding[0] = 0.0f;
	pointLightData->padding[1] = 0.0f;

	// ------------------------------
	// スポットライト用リソースを作る
	// ------------------------------

	// スポットライト用ConstantBufferを作成
	auto spotLightResourceOwner = dxCommon.CreateBufferResource(sizeof(SpotLight));
	ID3D12Resource* spotLightResource = spotLightResourceOwner.Get();
	// CPUから書き込むためのポインタ
	SpotLight* spotLightData = nullptr;
	// ResourceをMapして書き込み先を取得
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));
	// 色
	spotLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 位置
	spotLightData->position = { 2.0f, 1.25f, 0.0f };
	// 明るさ
	spotLightData->intensity = 4.0f;
	// 向き
	spotLightData->direction = { -1.0f, -1.0f, 0.0f };
	// 最大距離
	spotLightData->distance = 7.0f;
	// 減衰率
	spotLightData->decay = 2.0f;
	// 角度
	spotLightData->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	// 角度の減衰が始まる角度
	spotLightData->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 6.0f);
	// 角度の減衰が始まる角度
	float spotLightAngle = 60.0f;
	float spotLightFalloffStartAngle = 30.0f;
	// padding
	spotLightData->padding[0] = 0.0f;
	spotLightData->padding[1] = 0.0f;

	// ------------------------------
	// Light ON / OFF
	// ------------------------------

	bool useDirectionalLight = true;
	bool usePointLight = true;

	// ImGuiで編集する本来の強さ
	float directionalLightIntensity = 1.0f;
	float pointLightIntensity = 1.0f;

	// -------------------------------------
	// TextureManagerの初期化
	// -------------------------------------

	TextureManager textureManager;
	textureManager.Initialize(&dxCommon);

	// -------------------------------------
	// テクスチャを一括管理
	// -------------------------------------

	// 1枚目：circle
	uint32_t circleTextureIndex =
		textureManager.LoadTexture(
			"resources/circle.png"
		);

	// 2枚目：monsterBall
	uint32_t monsterTextureIndex =
		textureManager.LoadTexture(
			"resources/monsterBall.png"
		);

	// 3枚目：2個目のモデル用
	uint32_t modelTextureIndex =
		textureManager.LoadTexture(
			modelData2.material.textureFilePath
		);

	// -------------------------------------
	// 3Dモデル描画用GPUハンドル
	// -------------------------------------

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = textureManager.GetGPUHandle(circleTextureIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = textureManager.GetGPUHandle(monsterTextureIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = textureManager.GetGPUHandle(modelTextureIndex);

	// どちらのTextureを使うか
	bool useMonsterBall = true;

	// -----------------
	// Instancing用SRV
	// -----------------

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};

	instancingSrvDesc.Format =
		DXGI_FORMAT_UNKNOWN;

	instancingSrvDesc.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	instancingSrvDesc.ViewDimension =
		D3D12_SRV_DIMENSION_BUFFER;

	instancingSrvDesc.Buffer.FirstElement = 0;

	instancingSrvDesc.Buffer.Flags =
		D3D12_BUFFER_SRV_FLAG_NONE;

	instancingSrvDesc.Buffer.NumElements =
		ParticleSystem::kNumMaxInstance;

	instancingSrvDesc.Buffer.StructureByteStride =
		sizeof(ParticleForGPU);

	// 3番を使用
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon.GetSRVCPUDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = dxCommon.GetSRVGPUDescriptorHandle(3);

	// SRV作成
	device->CreateShaderResourceView(
		instancingResource,
		&instancingSrvDesc,
		instancingSrvHandleCPU
	);

	// =====================================
	// SpriteCommon初期化
	// =====================================

	SpriteCommon spriteCommon;
	spriteCommon.Initialize(&dxCommon);

	// =====================================
	// 1枚目のSprite初期化
	// =====================================

	Sprite sprite1;

	sprite1.Initialize(
		&spriteCommon,
		textureManager.GetGPUHandle(circleTextureIndex),
		textureManager.GetTextureSize(circleTextureIndex)
	);

	// 左上から50px、50px
	sprite1.SetPosition({ 50.0f, 50.0f });

	// 描画サイズ
	sprite1.SetSize({ 200.0f, 200.0f });

	// =====================================
	// 2枚目のSprite初期化
	// =====================================

	Sprite sprite2;

	sprite2.Initialize(
		&spriteCommon,
		textureManager.GetGPUHandle(monsterTextureIndex),
		textureManager.GetTextureSize(monsterTextureIndex)
	);

	// 個別の座標
	sprite2.SetPosition({ 350.0f, 150.0f });

	// 描画サイズ
	sprite2.SetSize({ 200.0f, 200.0f });

	// =====================================
	// 2枚目は画像の左上4分の1を使用
	// =====================================

	Vector2 monsterTextureSize = textureManager.GetTextureSize(monsterTextureIndex);

	sprite2.SetTextureLeftTop({ 0.0f, 0.0f });

	sprite2.SetTextureSize({
		monsterTextureSize.x * 0.5f,
		monsterTextureSize.y * 0.5f
		});

	// 最初のフレームでGPU転送完了後に解放する
	bool isSpriteTextureUploadReleased = false;

	// ------------------------------
	// ゲームループ
	// ------------------------------

	// ウィンドウのXボタンが押されるまでループする
	while (!winApp.IsEndRequested()) {

#ifdef USE_IMGUI

		// ImGuiのフレーム開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

#endif

		// ------------------------------
		// 入力更新
		// ------------------------------

		input->Update();

		if (input->PushKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");
		}

		// ------------------------------
		// デバッグカメラ更新
		// ------------------------------

		bool isImGuiUsingMouse = false;

#ifdef USE_IMGUI

		// ImGuiがマウス入力を使っているか
		isImGuiUsingMouse = ImGui::GetIO().WantCaptureMouse;

#endif

		// ImGui操作中はCameraを動かさない
		if (!isImGuiUsingMouse) {
			debugCamera.Update(input);
		}

		// Camera位置をGPUへ送る
		cameraData->worldPosition = debugCamera.GetWorldPosition();

		// ------------------------------
		// 3Dモデル用行列更新
		// ------------------------------

		// ワールド行列を作る
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		// DebugCameraからビュー行列を取得
		Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
		// DebugCameraから射影行列を取得
		Matrix4x4 projectionMatrix = debugCamera.GetProjectionMatrix();
		// ワールド行列、ビュー行列、射影行列を合成してWVP行列を作る
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		// World行列の逆行列を作る
		Matrix4x4 worldInverseMatrix = Inverse(worldMatrix);
		Matrix4x4 worldInverseTranspose = MakeIdentity4x4();
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {

				worldInverseTranspose.m[y][x] =
					worldInverseMatrix.m[x][y];
			}
		}
		// GPUへ転送
		wvpData->WVP = worldViewProjectionMatrix;
		// Lightingで使用するWorld行列
		wvpData->World = worldMatrix;
		// Lightingで使用するWorldの逆行列の転置行列
		wvpData->WorldInverseTranspose = worldInverseTranspose;

		// ------------------------------
		// 2個目のモデル行列更新
		// ------------------------------

		// 自分で設定しているTransform
		Matrix4x4 modelWorldMatrix2 = MakeAffineMatrix(transform2.scale, transform2.rotate, transform2.translate);
		// RootNodeのTransformを適用
		Matrix4x4 worldMatrix2 = Multiply(modelData2.rootNode.localMatrix, modelWorldMatrix2);
		// WVP
		Matrix4x4 worldViewProjectionMatrix2 = Multiply(worldMatrix2, Multiply(viewMatrix, projectionMatrix));
		// 逆行列
		Matrix4x4 worldInverseMatrix2 = Inverse(worldMatrix2);
		// 逆転置
		Matrix4x4 worldInverseTranspose2 = MakeIdentity4x4();
		for (int y = 0; y < 4; ++y) {

			for (int x = 0; x < 4; ++x) {

				worldInverseTranspose2.m[y][x] =
					worldInverseMatrix2.m[x][y];
			}
		}
		// GPUへ送る
		wvpData2->WVP = worldViewProjectionMatrix2;
		wvpData2->World = worldMatrix2;
		wvpData2->WorldInverseTranspose = worldInverseTranspose2;

		// -----------------
		// BillboardMatrix
		// -----------------

		// 板ポリの裏表を反転
		Matrix4x4 backToFrontMatrix =
			MakeRotateYMatrix(
				std::numbers::pi_v<float>
			);

		// View行列からCameraの回転行列を作る
		Matrix4x4 cameraMatrix = MakeIdentity4x4();

		// Viewの回転部分を転置する
		cameraMatrix.m[0][0] = viewMatrix.m[0][0];
		cameraMatrix.m[0][1] = viewMatrix.m[1][0];
		cameraMatrix.m[0][2] = viewMatrix.m[2][0];

		cameraMatrix.m[1][0] = viewMatrix.m[0][1];
		cameraMatrix.m[1][1] = viewMatrix.m[1][1];
		cameraMatrix.m[1][2] = viewMatrix.m[2][1];

		cameraMatrix.m[2][0] = viewMatrix.m[0][2];
		cameraMatrix.m[2][1] = viewMatrix.m[1][2];
		cameraMatrix.m[2][2] = viewMatrix.m[2][2];

		// Billboard
		Matrix4x4 billboardMatrix =
			Multiply(
				backToFrontMatrix,
				cameraMatrix
			);

		// 平行移動は使わない
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;

		// -----------------
		// Particle行列更新
		// -----------------

		Matrix4x4 viewProjectionMatrix =
			Multiply(
				viewMatrix,
				projectionMatrix
			);
		
		// -----------------
		// Emitter更新
		// -----------------

		emitter.frequencyTime += kDeltaTime;

		if (emitter.frequency <=
			emitter.frequencyTime) {

			particleSystem.Emit(
				emitter
			);

			emitter.frequencyTime -= emitter.frequency;
		}

		// Particle更新
		particleSystem.Update(
			viewProjectionMatrix,
			billboardMatrix,
			instancingData,
			kDeltaTime,
			useBillboard,
			useAccelerationField
		);

		// 描画するParticle数
		uint32_t numInstance = particleSystem.GetNumInstance();

		// -----------------
		// Particle UVAnimation
		// -----------------

		// 横方向にUVを流す
		particleUVTransform.translate.x +=
			0.2f * kDeltaTime;

		// 1.0を超えたら戻す
		if (particleUVTransform.translate.x >= 1.0f) {
			particleUVTransform.translate.x -= 1.0f;
		}

		// -----------------
		// Particle用UV行列
		// -----------------

		Matrix4x4 particleUVMatrix =
			MakeScaleMatrix(
				particleUVTransform.scale
			);

		particleUVMatrix =
			Multiply(
				particleUVMatrix,
				MakeRotateZMatrix(
					particleUVTransform.rotate.z
				)
			);

		particleUVMatrix =
			Multiply(
				particleUVMatrix,
				MakeTranslateMatrix(
					particleUVTransform.translate
				)
			);

		// GPUへ送る
		particleMaterialData->uvTransform = particleUVMatrix;

		// ------------------------------------
		// Sprite更新
		// ------------------------------------

		sprite1.Update();
		sprite2.Update();

#ifdef USE_IMGUI

		// ImGuiのウィンドウを表示する
		ImGui::Begin("Material");

		// -----------------
		// 球のスケール
		// -----------------
		ImGui::DragFloat3(
			"SphereScale",
			&transform.scale.x,
			0.01f,
			0.01f,
			10.0f
		);

		// モデルの回転角度を編集する
		ImGui::DragFloat3("ModelRotate", &transform.rotate.x, 0.01f);

		// 色を編集する
		ImGui::ColorEdit4("Color", &materialData->color.x);
		const char* blendModeNames[] = {
			"kBlendModeNone",
			"kBlendModeNormal",
			"kBlendModeAdd",
			"kBlendModeSub",
			"kBlendModeMul",
			"kBlendModeScreen"
		};

		int blendModeIndex = static_cast<int>(blendMode);

		if (ImGui::Combo("Blend", &blendModeIndex, blendModeNames, kCountOfBlendMode)) {

			blendMode = static_cast<BlendMode>(blendModeIndex);
		}

		// どちらのTextureを使うかのフラグ
		ImGui::Checkbox("useMonsterBall", &useMonsterBall);

		// -----------------
		// DirectionalLight
		// -----------------

		// 色
		ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
		// 向き
		if (ImGui::DragFloat3("Light Direction",&directionalLightData->direction.x,0.01f)) {
			directionalLightData->direction = Normalize(directionalLightData->direction);
		}
		// 強さ
		ImGui::DragFloat("DirectionalLight Intensity", &directionalLightIntensity, 0.01f, 0.0f, 10.0f);
		// 鏡面反射の鋭さ
		ImGui::DragFloat("Shininess", &materialData->shininess, 1.0f, 1.0f, 256.0f);
		// On / Off
		ImGui::Checkbox("DirectionalLight", &useDirectionalLight);

		// -----------------
		// PointLight
		// -----------------
		
		// 色
		ImGui::ColorEdit4("PointLight Color", &pointLightData->color.x);
		// 位置
		ImGui::DragFloat3("PointLight Position", &pointLightData->position.x, 0.01f, -10.0f, 10.0f);
		// 強さ
		ImGui::DragFloat("PointLight Intensity", &pointLightIntensity, 0.01f, 0.0f, 10.0f);
		// 有効範囲
		ImGui::DragFloat("PointLight Radius", &pointLightData->radius, 0.1f, 0.1f, 100.0f);
		// 減衰率
		ImGui::DragFloat("PointLight Decay", &pointLightData->decay, 0.01f, 0.1f, 10.0f);
		// On / Off
		ImGui::Checkbox("PointLight", &usePointLight);

		// -----------------
		// SpotLight
		// -----------------

		// 色
		ImGui::ColorEdit4("SpotLight Color", &spotLightData->color.x);
		// 位置
		ImGui::DragFloat3("SpotLight Position", &spotLightData->position.x, 0.01f, -20.0f, 20.0f);
		// 向き
		if (ImGui::DragFloat3("SpotLight Direction", &spotLightData->direction.x, 0.01f)) {
			spotLightData->direction = Normalize(spotLightData->direction);
		}
		// 強さ
		ImGui::DragFloat("SpotLight Intensity", &spotLightData->intensity, 0.01f, 0.0f, 10.0f);
		// 最大距離
		ImGui::DragFloat("SpotLight Distance", &spotLightData->distance, 0.1f, 0.1f, 100.0f);
		// 減衰率
		ImGui::DragFloat("SpotLight Decay", &spotLightData->decay, 0.01f, 0.1f, 10.0f);
		// 角度
		ImGui::DragFloat("SpotLight Angle", &spotLightAngle, 1.0f, 1.0f, 179.0f);
		// 減衰開始角度の余弦
		ImGui::DragFloat("SpotLight FalloffStart", &spotLightFalloffStartAngle, 1.0f, 0.0f, spotLightAngle - 0.1f);

		// Degree → cos
		spotLightData->cosAngle = std::cos(spotLightAngle * std::numbers::pi_v<float> / 180.0f);
		spotLightData->cosFalloffStart = std::cos(spotLightFalloffStartAngle * std::numbers::pi_v<float> / 180.0f);

		ImGui::End();

		// Light ON / OFFを反映
		if (useDirectionalLight) {
			directionalLightData->intensity = directionalLightIntensity;
		}
		else {
			directionalLightData->intensity = 0.0f;
		}

		if (usePointLight) {
			pointLightData->intensity = pointLightIntensity;
		}
		else {
			pointLightData->intensity = 0.0f;
		}

		// ImGuiの描画を行う
		ImGui::Render();

#endif



		// Windowにメッセージが来ていたら最優先で処理させる
		if (winApp.ProcessMessage()) {
			// メッセージがある場合はWinApp側で処理
		}
		else {

			// ---------------------------------
			// 描画前処理
			// ---------------------------------
			dxCommon.PreDraw();

			// RootSignatureを設定
			commandList->SetGraphicsRootSignature(rootSignature);

			// PSOを設定
			commandList->SetPipelineState(graphicsPipelineStates[blendMode]);

			// 描画する形状を三角形リストに設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// ---------------------
			// OBJモデルの描画
			// ---------------------

			// OBJモデル用の頂点バッファを設定する
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

			// OBJモデル用Materialを設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			// OBJモデル用TransformationMatrixを設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

			// 平行光源を設定
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

			// Cameraの位置を設定
			commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			// 点光源を設定
			commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());

			// スポットライトを設定
			commandList->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

			// 使うTextureを設定
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

			// OBJモデルを描画する
			commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

			// フラグが変わってもspriteを変えない
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// ------------------------------------
			// 2個目 : OBJ
			// ------------------------------------

			commandList->IASetVertexBuffers(0, 1, &vertexBufferView2);

			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			commandList->SetGraphicsRootConstantBufferView(1, wvpResource2->GetGPUVirtualAddress());

			// 2個目OBJのmtl Texture
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);

			// Lightは共有
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

			commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());

			commandList->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

			commandList->DrawInstanced(static_cast<UINT>(modelData2.vertices.size()), 1, 0, 0);

			// -----------------
			// Particle描画
			// -----------------

			// Particle用RootSignature
			commandList->SetGraphicsRootSignature(
				particleRootSignature
			);

			// Particle用PSO
			commandList->SetPipelineState(
				particlePipelineState
			);

			// Particle用頂点バッファ
			commandList->IASetVertexBuffers(
				0,
				1,
				&particleVertexBufferView
			);

			// Material
			commandList->SetGraphicsRootConstantBufferView(
				0,
				particleMaterialResource->GetGPUVirtualAddress()
			);

			// Instancing用StructuredBuffer
			commandList->SetGraphicsRootDescriptorTable(
				1,
				instancingSrvHandleGPU
			);

			// uvChecker
			commandList->SetGraphicsRootDescriptorTable(
				2,
				textureSrvHandleGPU
			);

			// 生存しているパーティクルだけ描画
			//if (numInstance > 0) {

			//	commandList->DrawInstanced(
			//		static_cast<UINT>(
			//			particleModelData.vertices.size()
			//			),
			//		numInstance,
			//		0,
			//		0
			//	);
			//}

			// =====================================
			// Sprite描画
			// =====================================

			// スプライト用RootSignature・PSOを設定
			spriteCommon.CommonDraw();

			// 1枚目を描画
			sprite1.Draw();

			// 2枚目を描画
			sprite2.Draw();

#ifdef USE_IMGUI
			// ImGuiの描画を行う
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

			// ---------------------------------
			// 描画後処理
			// ---------------------------------
			dxCommon.PostDraw();

			// Spriteテクスチャ転送用リソース解放
			if (!isSpriteTextureUploadReleased) {

				textureManager.ReleaseUploadResources();

				isSpriteTextureUploadReleased = true;
			}
		}
	}

	// ---------------------------------
	// RootSignature・PSOの解放
	// ---------------------------------

	// Particle用PSO
	particlePipelineStateOwner.Reset();

	// Particle用RootSignature
	particleRootSignatureOwner.Reset();

	// Particle用シリアライズ結果
	particleSignatureBlob.Reset();
	particleErrorBlob.Reset();

	// Object3D用PSO
	for (uint32_t i = 0; i < kCountOfBlendMode; ++i) {
		graphicsPipelineStatesOwner[i].Reset();
	}

	// Object3D用RootSignature
	rootSignatureOwner.Reset();

	// Object3D用シリアライズ結果
	signatureBlob.Reset();
	errorBlob.Reset();

	// コンパイル済みShader
	vertexShaderBlob.Reset();
	pixelShaderBlob.Reset();
	particleVertexShaderBlob.Reset();
	particlePixelShaderBlob.Reset();

	// ------------------------------
	// ComPtrで所有しているリソースの解放
	// ------------------------------

	// モデル関連
	vertexResourceOwner.Reset();
	vertexResource2Owner.Reset();
	wvpResourceOwner.Reset();
	wvpResource2Owner.Reset();
	materialResourceOwner.Reset();

	// Particle関連
	particleVertexResourceOwner.Reset();
	instancingResourceOwner.Reset();
	particleMaterialResourceOwner.Reset();

	// Camera・Light関連
	cameraResourceOwner.Reset();
	directionalLightResourceOwner.Reset();
	pointLightResourceOwner.Reset();
	spotLightResourceOwner.Reset();

	// TextureManagerの終了
	textureManager.Finalize();

	// DirectX12基盤の終了処理
	dxCommon.Finalize();

#ifdef _DEBUG
	debugController.Reset();
#endif

	// ウィンドウの終了処理
	winApp.Finalize();

	// Inputクラスのインスタンスを解放
	delete input;
	input = nullptr;

	// Audioの終了
	audio.Finalize();

	// Loggerの終了
	Logger::Finalize();

	// COMの終了
	CoUninitialize();

	return 0;
}