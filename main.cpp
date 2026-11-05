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
#include "Camera.h"
#include "BlendMode.h"
#include "ParticleSystem.h"
#include "ParticleManager.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
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
#include <utility>

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

	// カメラの初期化
	Camera camera;
	camera.Initialize();

	// ------------------------------
	// Camera切り替え用
	// ------------------------------

	// true : DebugCamera
	// false : 通常Camera
	bool useDebugCamera = true;

	// 通常Cameraの座標
	Vector3 cameraPosition = {
		0.0f, 0.0f, -50.0f
	};

	// 通常Cameraの回転
	Vector3 cameraRotation = {
		0.0f, 0.0f, 0.0f
	};

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

	// ---------------------------------
	// SrvManager初期化
	// ---------------------------------

	SrvManager srvManager;
	srvManager.Initialize(&dxCommon);

	// ---------------------------------
	// ImGuiManagerの初期化
	// ---------------------------------

	ImGuiManager imguiManager;
	imguiManager.Initialize(&winApp, &dxCommon, &srvManager);

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

	// --------------------------------------
	// ParticleManagerの初期化
	// --------------------------------------

	ParticleManager particleManager;

	particleManager.Initialize(
		&dxCommon,
		&srvManager
	);

	// 3Dモデル用ブレンドモード
	BlendMode blendMode = kBlendModeNormal;

	// --------------------------------------
	// AccelerationField
	// --------------------------------------
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

	particleManager.SetAccelerationField(
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

	// Transform変数を作る
	Transform transform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, -1.54f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

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
	textureManager.Initialize(&dxCommon, &srvManager);

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

	// -------------------------------------
	// 3Dモデル描画用GPUハンドル
	// -------------------------------------

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = textureManager.GetGPUHandle(circleTextureIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = textureManager.GetGPUHandle(monsterTextureIndex);

	// どちらのTextureを使うか
	bool useMonsterBall = true;

	// -------------------------------------
	// Particle描画用テクスチャ設定
	// -------------------------------------

	// circle.pngをParticleに設定
	particleManager.SetTextureHandle(
		textureManager.GetGPUHandle(
			circleTextureIndex
		)
	);

	// -------------------------------------
	// 2つ目のParticleグループを作成
	// -------------------------------------

	// monsterBall.pngを使用するグループ
	particleManager.CreateParticleGroup(
		"monster",
		textureManager.GetGPUHandle(
			monsterTextureIndex
		)
	);

	// monsterグループ用Emitter
	Emitter monsterEmitter = emitter;

	// circleと違う位置に発生
	monsterEmitter.transform.translate = {
		4.0f, 0.0f, 0.0f
	};

	// 発生タイマーは別々に管理
	monsterEmitter.frequencyTime = 0.0f;

	// -------------------------------------
	// Object3dCommon初期化
	// -------------------------------------

	Object3dCommon object3dCommon;
	object3dCommon.Initialize(&dxCommon);

	// 共通Cameraを登録
	object3dCommon.SetDefaultCamera(&camera);

	// -------------------------------------
	// ModelManager初期化
	// -------------------------------------

	ModelManager modelManager;
	modelManager.Initialize(
		&dxCommon,
		&textureManager
	);

	// -------------------------------------
	// 球モデルをModelManagerに登録
	// -------------------------------------

	Model* sphereModel =
		modelManager.RegisterGeneratedModel(
			"sphere",
			std::move(modelData),
			circleTextureIndex
		);

	assert(sphereModel != nullptr);

	// 現在のImGuiが編集するMaterial
	Material* materialData = sphereModel->GetMaterialData();

	assert(materialData != nullptr);

	// -------------------------------------
	// 球のObject3dを生成
	// -------------------------------------

	Object3d sphereObject;
	sphereObject.Initialize(&object3dCommon);

	sphereObject.SetModel(sphereModel);
	sphereObject.SetTransform(transform);

	// -------------------------------------
	// 3Dモデル読み込み
	// -------------------------------------

	// ModelManagerがplane.gltfを管理する
	Model* planeModel = modelManager.LoadModel(
		"resources/plane",
		"plane.gltf"
	);

	assert(planeModel != nullptr);

	// 2つ目のModelを読み込む
	Model* secondModel = modelManager.LoadModel(
		"resources/fence",
		"fence.obj"
	);

	assert(secondModel != nullptr);

	// 異なるモデルが登録されていることを確認
	assert(planeModel != secondModel);

	// -------------------------------------
	// 1個目の3Dオブジェクト
	// -------------------------------------

	Object3d object1;
	object1.Initialize(&object3dCommon);

	object1.SetModel(planeModel);
	object1.SetPosition({ 3.0f, 0.0f, 0.0f });
	object1.SetRotation({ 0.0f, -1.54f, 0.0f });
	object1.SetScale({ 1.0f, 1.0f, 1.0f });

	// -------------------------------------
	// 2個目の3Dオブジェクト
	// -------------------------------------

	Object3d object2;
	object2.Initialize(&object3dCommon);

	// 1個目と同じModelを共有
	object2.SetModel(secondModel);

	object2.SetPosition({ 6.0f, 0.0f, 0.0f });
	object2.SetRotation({ 0.0f, -1.54f, 0.0f });
	object2.SetScale({ 1.0f, 1.0f, 1.0f });

	// -------------------------------------
	// SpriteCommon初期化
	// -------------------------------------

	SpriteCommon spriteCommon;
	spriteCommon.Initialize(&dxCommon);

	// -------------------------------------
	// 1枚目のSprite初期化
	// -------------------------------------

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

	// -------------------------------------
	// 2枚目のSprite初期化
	// -------------------------------------

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

	// -------------------------------------
	// 2枚目は画像の左上4分の1を使用
	// -------------------------------------

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

		// ImGuiのフレーム開始
		imguiManager.Begin();

		// ------------------------------
		// 入力更新
		// ------------------------------

		input->Update();

		if (input->PushKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");
		}

		// ------------------------------------
		// Camera更新
		// ------------------------------------

		// ImGuiがマウス操作中か確認
		bool isImGuiUsingMouse = false;

#ifdef USE_IMGUI
		isImGuiUsingMouse = ImGui::GetIO().WantCaptureMouse;
#endif

		if (useDebugCamera) {

			// ---------------------------------
			// DebugCameraを使用
			// ---------------------------------

			if (!isImGuiUsingMouse) {
				debugCamera.Update(input);
			}

			// DebugCameraのView行列を共通Cameraへ反映
			camera.SetViewMatrix(
				debugCamera.GetViewMatrix()
			);

		}
		else {

			// ---------------------------------
			// 通常Cameraを使用
			// ---------------------------------

			// 座標を設定
			camera.SetTranslate(cameraPosition);

			// 回転を設定
			camera.SetRotate(cameraRotation);

			// 行列を更新
			camera.Update();
		}

		// Cameraの座標をGPUに送る
		cameraData->worldPosition =
			camera.GetTranslate();

		// ------------------------------------
		// 共通Cameraの行列を取得
		// Particle描画でも使用する
		// ------------------------------------

		Matrix4x4 viewMatrix =
			camera.GetViewMatrix();

		Matrix4x4 projectionMatrix =
			camera.GetProjectionMatrix();

		// ------------------------------------
		// 3Dオブジェクト更新
		// ------------------------------------

		// 球のTransformを反映
		sphereObject.SetTransform(transform);

		// 球
		sphereObject.Update();

		// plane
		object1.Update();

		// fence
		object2.Update();

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

			particleManager.Emit(
				emitter
			);

			emitter.frequencyTime -= emitter.frequency;
		}

		// ------------------------------
		// monsterグループの発生処理
		// ------------------------------

		monsterEmitter.frequencyTime += kDeltaTime;

		if (monsterEmitter.frequency <=
			monsterEmitter.frequencyTime) {

			particleManager.Emit(
				"monster",
				monsterEmitter
			);

			monsterEmitter.frequencyTime -=
				monsterEmitter.frequency;
		}

		// Particle更新
		particleManager.Update(
			viewProjectionMatrix,
			billboardMatrix,
			kDeltaTime,
			useBillboard,
			useAccelerationField
		);

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
		particleManager.SetUVTransform(
			particleUVMatrix
		);

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

		// Camera操作
		ImGui::SeparatorText("Camera");

		// DebugCamera / 通常Camera切り替え
		ImGui::Checkbox(
			"Use DebugCamera",
			&useDebugCamera
		);

		// 通常Camera使用中だけ座標・回転を編集
		if (!useDebugCamera) {

			ImGui::DragFloat3(
				"Camera Position",
				&cameraPosition.x,
				0.1f
			);

			ImGui::DragFloat3(
				"Camera Rotation",
				&cameraRotation.x,
				0.01f
			);
		}

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

		// ImGuiのフレーム終了
		imguiManager.End();

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

			// -------------------------------------
			// Object3dによる3Dモデル描画
			// -------------------------------------

			// 3Dオブジェクト共通のRootSignature・PSO
			object3dCommon.CommonDraw(blendMode);

			// -------------------------------------
			// Camera・LightのConstantBuffer
			// -------------------------------------

			// 平行光源
			commandList->SetGraphicsRootConstantBufferView(
				3,
				directionalLightResource->GetGPUVirtualAddress()
			);

			// カメラ
			commandList->SetGraphicsRootConstantBufferView(
				4,
				cameraResource->GetGPUVirtualAddress()
			);

			// 点光源
			commandList->SetGraphicsRootConstantBufferView(
				5,
				pointLightResource->GetGPUVirtualAddress()
			);

			// スポットライト
			commandList->SetGraphicsRootConstantBufferView(
				6,
				spotLightResource->GetGPUVirtualAddress()
			);

			// =====================================
			// 球モデルの描画
			// =====================================

			// 既存のテクスチャ切り替えを維持
			sphereModel->SetTextureIndex(
				useMonsterBall
				? monsterTextureIndex
				: circleTextureIndex
			);

			// 球を描画
			sphereObject.Draw();

			// -------------------------------------
			// 個別の3Dオブジェクトを描画
			// -------------------------------------

			object1.Draw();
			object2.Draw();

			// -------------------------------------
			// Particle描画
			// -------------------------------------

			particleManager.Draw();

			// -------------------------------------
			// Sprite描画
			// -------------------------------------

			// スプライト用RootSignature・PSOを設定
			spriteCommon.CommonDraw();

			// 1枚目を描画
			sprite1.Draw();

			// 2枚目を描画
			sprite2.Draw();

			// ImGuiの描画
			imguiManager.Draw();

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

	// Camera・Light関連
	cameraResourceOwner.Reset();
	directionalLightResourceOwner.Reset();
	pointLightResourceOwner.Reset();
	spotLightResourceOwner.Reset();

	// ParticleManagerの終了処理
	particleManager.Finalize();

	// Object3dが所有するWVPを解放
	sphereObject.Finalize();
	object1.Finalize();
	object2.Finalize();

	// ModelManagerがモデルと頂点リソースを解放
	modelManager.Finalize();

	// 3D共通RootSignature・PSOを解放
	object3dCommon.Finalize();

	// TextureManagerの終了
	textureManager.Finalize();

	// ImGuiManager終了
	imguiManager.Finalize();

	// SrvManager終了
	srvManager.Finalize();

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