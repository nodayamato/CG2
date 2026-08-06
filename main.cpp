#pragma warning(push)
// C4023の警告を無効化する
#pragma warning(disable:4023)
#include <Windows.h>
#include "ConvertString.h"
#include "Math.h"
#include "ModelData.h"
#include <cstring>
// 標準入出力を扱うライブラリ
#include <cstdint>
// 文字列を扱うライブラリ
#include <string>
// ファイルやディレクトリに関する操作を行うライブラリ
#include <filesystem>
// ファイルに書いたり読むためのライブラリ
#include <fstream>
// 時間に関するライブラリ
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cassert>
// デバッグ用のライブラリ
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <vector>
#include <numbers>
#pragma warning(pop)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

// ImGuiのWndProcHandlerを宣言する
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
);

#endif

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
	WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI

	// ImGuiのWndProcHandlerにメッセージを渡す
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

#endif

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {

		// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

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

/// <summary>
/// ログファイルとデバッグ出力に文字列を出力する
/// </summary>
/// <param name="os">出力先のストリーム</param>
/// <param name="message">出力する文字列</param>
void Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	// デバッグ出力
	OutputDebugStringA(message.c_str());
}

/// <summary>
/// デバッグ出力を行う
/// </summary>
/// <param name="message">出力する文字列</param>
void Log(const std::string& message)
{
	OutputDebugStringA(message.c_str());
}

/// <summary>
/// デバッグ出力を行う（wstring版）
/// </summary>
/// <param name="message">出力する文字列</param>
void Log(const std::wstring& message)
{
	Log(ConvertString(message));
}

/// <summary>
/// HLSLファイルをコンパイルする
/// </summary>
/// <param name="filePath">コンパイルするHLSLファイルのパス</param>
/// <param name="profile">コンパイルするシェーダープロファイル</param>
/// <param name="dxcUtils">DXCユーティリティ</param>
/// <param name="dxcCompiler">DXCコンパイラ</param>
/// <param name="includeHandler">Includeハンドル</param>
/// <returns>コンパイル結果</returns>
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	// これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(
		L"Begin CompileShader, path:{}, profile:{}\n",
		filePath,
		profile)));

	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(
		filePath.c_str(),
		nullptr,
		&shaderSource);

	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer{};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
	filePath.c_str(),
	L"-E", L"main",
	L"-T", profile,
	L"-Zi",
	L"-Qembed_debug",
	L"-Od",
	L"-Zpr",
	};

	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;

	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult));

	assert(SUCCEEDED(hr));

	// 警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;

	shaderResult->GetOutput(
		DXC_OUT_ERRORS,
		IID_PPV_ARGS(&shaderError),
		nullptr);

	if (shaderError != nullptr &&
		shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());

		// 警告・エラーダメゼッタイ
		assert(false);
	}

	// コンパイル結果から実行用バイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;

	hr = shaderResult->GetOutput(
		DXC_OUT_OBJECT,
		IID_PPV_ARGS(&shaderBlob),
		nullptr);

	assert(SUCCEEDED(hr));

	// 成功したログを出す
	Log(ConvertString(std::format(
		L"Compile Succeeded, path:{}, profile:{}\n",
		filePath,
		profile)));

	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();

	// 実行用バイナリを返却
	return shaderBlob;
}

/// <summary>
/// バッファリソースを作成する
/// </summary>
/// <param name="device">D3D12デバイス</param>
/// <param name="sizeInBytes">作成するバッファのサイズ</param>
/// <returns>作成したバッファリソース</returns>
ID3D12Resource* CreateBufferResource(
	ID3D12Device* device,
	size_t sizeInBytes)
{
	// 頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// バッファリソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// バッファリソースを生成
	ID3D12Resource* resource = nullptr;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource));

	assert(SUCCEEDED(hr));

	return resource;
}

/// <summary>
/// ディスクリプターヒープを作成する
/// </summary>
/// <param name="device">D3D12デバイス</param>
/// <param name="heapType">ヒープのタイプ</param>
/// <param name="numDescriptors">ディスクリプタの数</param>
/// <param name="shaderVisible">シェーダーから可視かどうか</param>
/// <returns>作成したディスクリプターヒープ</returns>
ID3D12DescriptorHeap* CreateDescriptorHeap(
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisible)
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags =
		shaderVisible ?
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(
		&descriptorHeapDesc,
		IID_PPV_ARGS(&descriptorHeap));

	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

/// <summary>
/// テクスチャを読み込む
/// </summary>
/// <param name="filePath">テクスチャファイルのパス</param>
/// <returns>読み込まれたテクスチャデータ</returns>
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(
		filePathW.c_str(),
		DirectX::WIC_FLAGS_FORCE_SRGB,
		nullptr,
		image);
	assert(SUCCEEDED(hr));

	// ミップマップを作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB,
		0,
		mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;
}

/// <summary>
/// テクスチャリソースを作成する
/// </summary>
/// <param name="device">D3D12デバイス</param>
/// <param name="metadata">テクスチャメタデータ</param>
/// <returns>作成したテクスチャリソース</returns>
ID3D12Resource* CreateTextureResource(
	ID3D12Device* device,
	const DirectX::TexMetadata& metadata)
{
	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);// Textureの幅
	resourceDesc.Height = UINT(metadata.height);// Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);// mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);// 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;// サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Textureの次元数

	// Heapの設定(VRAM)
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,// Heapの設定
		D3D12_HEAP_FLAG_NONE,// Heapの特殊な設定。特になし。
		&resourceDesc,// Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,// Resourceの最初の状態
		nullptr,// Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));// 作成するResourceポインタへのポインタ

	assert(SUCCEEDED(hr));

	return resource;
}

/// <summary>
/// テクスチャデータをアップロードする
/// </summary>
/// <param name="texture">テクスチャリソース</param>
/// <param name="mipImages">ミップマップ画像データ</param>
/// <param name="device">D3D12デバイス</param>
/// <param name="commandList">コマンドリスト</param>
/// <returns></returns>
[[nodiscard]]
ID3D12Resource* UploadTextureData(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList)
{
	// Subresource情報を作成
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(
		device,
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources);

	// IntermediateResourceに必要なサイズ
	uint64_t intermediateSize =
		GetRequiredIntermediateSize(
			texture,
			0,
			UINT(subresources.size()));

	// IntermediateResource作成
	ID3D12Resource* intermediateResource =
		CreateBufferResource(device, intermediateSize);

	// Textureへコピーするコマンドを積む
	UpdateSubresources(
		commandList,
		texture,
		intermediateResource,
		0,
		0,
		UINT(subresources.size()),
		subresources.data());

	// ResourceState変更
	D3D12_RESOURCE_BARRIER barrier{};

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// <summary>
/// DepthStencil用のTextureResourceを作成する
/// </summary>
/// <param name="device">D3D12デバイス</param>
/// <param name="width">Textureの幅</param>
/// <param name="height">Textureの高さ</param>
/// <returns>作成したDepthStencilTextureResource</returns>
ID3D12Resource* CreateDepthStencilTextureResource(
	ID3D12Device* device,
	int32_t width,
	int32_t height)
{
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};

	// Textureの幅
	resourceDesc.Width = width;

	// Textureの高さ
	resourceDesc.Height = height;

	// mipmapの数
	resourceDesc.MipLevels = 1;

	// 奥行き or 配列Textureの配列数
	resourceDesc.DepthOrArraySize = 1;

	// DepthStencilとして利用可能なフォーマット
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// サンプリングカウント。1固定
	resourceDesc.SampleDesc.Count = 1;

	// 2次元Texture
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// DepthStencilとして使うことを通知
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};

	// VRAM上に作る
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};

	// 1.0f（最大値）でクリア
	depthClearValue.DepthStencil.Depth = 1.0f;

	// ResourceのFormatと合わせる
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Resourceの生成
	ID3D12Resource* resource = nullptr;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,					// Heapの設定
		D3D12_HEAP_FLAG_NONE,				// Heapの特殊な設定。特になし
		&resourceDesc,						// Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	// 深度値を書き込む状態
		&depthClearValue,					// Clear最適値
		IID_PPV_ARGS(&resource));			// 作成するResourceポインタへのポインタ

	assert(SUCCEEDED(hr));

	return resource;
}

/// <summary>
/// CPUデスクリプタハンドルを取得する
/// </summary>
/// <param name="descriptorHeap">デスクリプタヒープ</param>
/// <param name="descriptorSize">デスクリプタのサイズ</param>
/// <param name="index">インデックス</param>
/// <returns>取得したCPUデスクリプタハンドル</returns>
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap,
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

/// <summary>
/// GPUデスクリプタハンドルを取得する
/// </summary>
/// <param name="descriptorHeap">デスクリプタヒープ</param>
/// <param name="descriptorSize">デスクリプタのサイズ</param>
/// <param name="index">インデックス</param>
/// <returns>取得したGPUデスクリプタハンドル</returns>
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap,
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

// Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// COMの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// Dump出力を設定
	SetUnhandledExceptionFilter(ExportDump);

	// ------------------------------
	// ウィンドウ関連の初期化
	// ------------------------------

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	WNDCLASS wc{};
	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	// ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,          // 利用するクラス名
		L"CG2",                    // タイトル
		WS_OVERLAPPEDWINDOW,       // ウィンドウスタイル
		CW_USEDEFAULT,             // 表示位置X
		CW_USEDEFAULT,             // 表示位置Y
		wrc.right - wrc.left,      // 幅
		wrc.bottom - wrc.top,      // 高さ
		nullptr,                   // 親ウィンドウ
		nullptr,                   // メニュー
		wc.hInstance,              // インスタンスハンドル
		nullptr                    // オプション
	);
	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	// デバッグレイヤーを有効化する
	ID3D12Debug1* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();

		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif

	// ------------------------------
	//  ログ関連の初期化
	// ------------------------------

	// logsフォルダを作る
	std::filesystem::create_directory("logs");
	// 現在時刻を取得（UTC）
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	// 秒単位に変換
	auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// ローカル時間へ変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
	// ファイル名用の日時文字列
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	// ログファイル名
	std::string logFilePath = "logs/" + dateString + ".log";
	// ログファイルを開く
	std::ofstream logStream(logFilePath);

	// ------------------------------
	// DirectX12関連の初期化
	// ------------------------------

	// DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;

	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	// 初期化の基本的な部分でエラーが出る場合はプログラムが間違っているか、
	// どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数。最初にnullptrを入れておく
	IDXGIAdapter4* useAdapter = nullptr;

	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {

		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得できないのは一大事

		// ソフトウェアアダプタでなければ採用！
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {

			// 採用したアダプタの情報をログに出力。wstringの方なので注意
			Log(std::format(L"Use Adapter:{}\n", adapterDesc.Description));

			break;
		}

		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}

	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	// D3D12デバイス
	ID3D12Device* device = nullptr;

	// 機能レベル
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
	};

	// ログ出力用文字列
	const char* featureLevelStrings[] = {
		"12.2",
		"12.1",
		"12.0",
	};

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		// 採用したアダプタでデバイスを生成
		hr = D3D12CreateDevice(
			useAdapter,
			featureLevels[i],
			IID_PPV_ARGS(&device));

		// 指定した機能レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr))
		{
			// 生成できたのでログ出力してループを抜ける
			Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}

	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);

	// デスクリプタのサイズを取得
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// 初期化完了のログ
	Log(logStream, "Complete create D3D12Device!!!\n");

#ifdef _DEBUG
	// デバッグレイヤーの情報を取得する
	ID3D12InfoQueue* infoQueue = nullptr;

	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

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
		infoQueue->Release();
	}

#endif

	// ------------------------------
	// コマンド周り
	// ------------------------------

	// コマンドキューを生成する
	ID3D12CommandQueue* commandQueue = nullptr;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	hr = device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue)
	);

	// コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドアロケータを生成する
	ID3D12CommandAllocator* commandAllocator = nullptr;

	hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);

	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	ID3D12GraphicsCommandList* commandList = nullptr;

	hr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator,
		nullptr,
		IID_PPV_ARGS(&commandList)
	);

	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// ------------------------------
	// スワップチェーン関連
	// ------------------------------

	// スワップチェーンを生成する
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	// 画面の幅。ウィンドウのクライアント領域と同じものにしておく
	swapChainDesc.Width = kClientWidth;
	// 画面の高さ。ウィンドウのクライアント領域と同じものにしておく
	swapChainDesc.Height = kClientHeight;
	// 色の形式
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// マルチサンプルしない
	swapChainDesc.SampleDesc.Count = 1;
	// 描画のターゲットとして利用する
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	// ダブルバッファ
	swapChainDesc.BufferCount = 2;
	// モニタにうつしたら、中身を破棄
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(&swapChain)
	);

	// 生成できなかったので起動できない
	assert(SUCCEEDED(hr));

	// ------------------------------
	// レンダーターゲットビューの生成
	// ------------------------------

	// RTV用のディスクリプタヒープを生成する
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のディスクリプタヒープを生成する
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// DepthStencilTextureをウィンドウのサイズで作成
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// DSV用のディスクリプタヒープを生成する
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	// Resourceと同じFormat
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 2DTexture
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	// DSVを生成
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	// SwapChainからResourceを引っ張ってくる
	ID3D12Resource* swapChainResources[2] = { nullptr };

	hr = swapChain->GetBuffer(
		0,
		IID_PPV_ARGS(&swapChainResources[0])
	);

	// うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));

	hr = swapChain->GetBuffer(
		1,
		IID_PPV_ARGS(&swapChainResources[1])
	);

	assert(SUCCEEDED(hr));

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む

	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 0);

	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// まず1つ目を作る。1つ目は最初のところに作る
	rtvHandles[0] = rtvStartHandle;

	device->CreateRenderTargetView(
		swapChainResources[0],
		&rtvDesc,
		rtvHandles[0]
	);

	// 2つ目のディスクリプタを得る（自力で）
	rtvHandles[1].ptr =
		rtvHandles[0].ptr +
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 2つ目を作る
	device->CreateRenderTargetView(
		swapChainResources[1],
		&rtvDesc,
		rtvHandles[1]
	);

	// 初期値0でFenceを作る
	ID3D12Fence* fence = nullptr;

	uint64_t fenceValue = 0;

	hr = device->CreateFence(
		fenceValue,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence)
	);

	assert(SUCCEEDED(hr));


	// FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL
	);

	assert(fenceEvent != nullptr);

	//------------------------------
	// DXC関連の初期化
	//------------------------------

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;

	hr = DxcCreateInstance(
		CLSID_DxcUtils,
		IID_PPV_ARGS(&dxcUtils)
	);
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(
		CLSID_DxcCompiler,
		IID_PPV_ARGS(&dxcCompiler)
	);
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;

	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	// ---------------------------------
	// GraphicsPipelineの初期化
	// ---------------------------------

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"Object3d.VS.hlsl",
		L"vs_6_0",
		dxcUtils,
		dxcCompiler,
		includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(
		L"Object3d.PS.hlsl",
		L"ps_6_0",
		dxcUtils,
		dxcCompiler,
		includeHandler);
	assert(pixelShaderBlob != nullptr);

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
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

	// シリアライズ
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);

	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

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

	// BlendState
	D3D12_BLEND_DESC blendDesc{};

	// 全色書き込み
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

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

	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// DepthStencilStateを設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	// DSVのFormatを設定
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTV情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// 利用するトポロジ
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプル数
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;

	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));

	assert(SUCCEEDED(hr));

	// ------------------------------
	// Sprite用頂点リソースを作る
	// ------------------------------

	// Sprite用の頂点リソースを作成
	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
	// Sprite用頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// GPUアドレス
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// バッファサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点サイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// ------------------------------
	// Sprite用IndexResourceを作る
	// ------------------------------

	// Sprite用IndexResourceを作成
	ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

	// Sprite用IndexBufferViewを作成
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};

	// IndexResourceのGPUアドレスを設定
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();

	// IndexBuffer全体のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;

	// Index1個の形式はuint32_t
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// ------------------------------
	// Sprite用頂点データ
	// ------------------------------

	VertexData* vertexDataSprite = nullptr;

	// 書き込み先取得
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 左下
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };

	// 左上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };

	// 右下
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };

	// 右上
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };

	// ------------------------------
	// Sprite用Indexデータ
	// ------------------------------

	// IndexResourceの書き込み先
	uint32_t* indexDataSprite = nullptr;

	// CPUから書き込めるようにMapする
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));

	// 1枚目の三角形
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;

	// 2枚目の三角形
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	// ------------------------------
	// OBJモデルを読み込む
	// ------------------------------

	// resourcesフォルダにあるplane.objを読み込む
	ModelData modelData = LoadObjFile("resources", "plane.obj");

	// OBJファイルに頂点が入っていることを確認する
	assert(!modelData.vertices.empty());

	// ------------------------------
	// モデル用頂点リソースを作成
	// ------------------------------

	// 読み込んだモデルの頂点数に合わせて頂点バッファを作る
	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

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
	ID3D12Resource* materialResource = CreateBufferResource(device, 256);

	// Map
	Material* materialData = nullptr;

	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 初期値
	materialData->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData->enableLighting = true;

	// 球のUVは最初は変形しない
	materialData->uvTransform = MakeIdentity4x4();

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));

	// データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	// 単位行列を書き込んでおく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

	// Transform変数を作る
	Transform transform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};
	// CameraTransform変数を作る
	Transform cameraTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -10.0f}
	};

	// ------------------------------
	// 平行光源用リソースを作る
	// ------------------------------

	// 平行光源用ConstantBufferを作成
	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));

	// CPUから書き込むためのポインタ
	DirectionalLight* directionalLightData = nullptr;

	// ResourceをMapして書き込み先を取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// ライトの色を白にする
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// ライトを上から下へ向ける
	// この値はすでに単位ベクトル
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };

	// ライトの明るさ
	directionalLightData->intensity = 1.0f;

	// --------------------------------
	// Sprite用TransformationMatrix
	// --------------------------------

	// Sprite用TransformationMatrix
	ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, 256);

	// Sprite用MaterialResource
	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, 256);

	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));

	materialDataSprite->color = { 1.0f,1.0f,1.0f,1.0f };
	materialDataSprite->enableLighting = false;

	// SpriteのUVも最初は変形しない
	materialDataSprite->uvTransform = MakeIdentity4x4();

	// Sprite用TransformationMatrixのデータを書き込むためのポインタ
	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	// Map
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	// 単位行列
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();

	// Sprite用Transform
	Transform transformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	// UVの拡縮・回転・平行移動を保持する
	Transform uvTransformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	// ------------------------------
	// 描画に必要な情報をまとめる
	// ------------------------------

	// ビューポート
	D3D12_VIEWPORT viewport{};

	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(kClientWidth);
	viewport.Height = static_cast<float>(kClientHeight);
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};

	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	// ------------------------------
	// Textureの読み込み
	// ------------------------------

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
	ID3D12Resource* intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);

	// 2枚目のTexture
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
	ID3D12Resource* intermediateResource2 = UploadTextureData(textureResource2, mipImages2, device, commandList);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// 先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの生成
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

	// 2枚目のSRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// インデックス2に作る
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	// SRVの生成
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

	// どちらのTextureを使うかのフラグ
	bool useMonsterBall = true;

	// ------------------------------
	// ImGuiの初期化
	// ------------------------------

#ifdef USE_IMGUI

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);

	ImGui_ImplDX12_Init(
		device,
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap,
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();

#endif

	// ------------------------------
	// ゲームループ
	// ------------------------------

	MSG msg{};
	// ウィンドウのXボタンが押されるまでループする
	while (msg.message != WM_QUIT) {

#ifdef USE_IMGUI

		// ImGuiのフレーム開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

#endif

		// ワールド行列を作る
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		// カメラの行列を作る
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		// ビュー行列を作る
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		// 射影行列を作る
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
		// ワールド行列、ビュー行列、射影行列を合成してWVP行列を作る
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		// GPUへ転送
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;

		// ---------------------
		// Sprite用WVP
		// ---------------------

		// World
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);

		// View
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4();

		// Projection
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);

		// WVP
		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

		// Sprite用WVP行列をGPUへ渡す
		transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;

		// Sprite用World行列をGPUへ渡す
		transformationMatrixDataSprite->World = worldMatrixSprite;

#ifdef USE_IMGUI

		// ImGuiのウィンドウを表示する
		//ImGui::ShowDemoWindow();

		// ImGuiのウィンドウを表示する
		ImGui::Begin("Material");

		// モデルの回転角度を編集する
		ImGui::DragFloat3("ModelRotate", &transform.rotate.x, 0.01f);

		// 色を編集する
		ImGui::ColorEdit4("Color", &materialData->color.x);
		// どちらのTextureを使うかのフラグ
		ImGui::Checkbox("useMonsterBall", &useMonsterBall);

		// ライトの色
		ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
		// ライトの向き
		ImGui::DragFloat3("Light Direction", &directionalLightData->direction.x, 0.01f);

		ImGui::End();

		// ImGuiのウィンドウを表示する
		ImGui::Begin("Sprite");

		// Sprite本体の座標を変更する
		ImGui::DragFloat3("Position", &transformSprite.translate.x, 1.0f);
		// UVの平行移動
		ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
		// UVの拡縮
		ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
		// UVのZ軸回転
		ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);

		ImGui::End();

		// ImGuiの描画を行う
		ImGui::Render();

#endif

		// ------------------------------
		// Sprite用UVTransform行列を作る
		// ------------------------------

		// 拡縮行列を作る
		Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
		// Z軸回転行列を掛ける
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
		// 平行移動行列を掛ける
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
		// Sprite用MaterialへUV変換行列を書き込む
		materialDataSprite->uvTransform = uvTransformMatrix;


		// Windowにメッセージが来ていたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};

			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex];

			// 遷移前（現在）のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のDSVを取得
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			// RTVとDSVを設定
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			// 指定した色で画面全体をクリアする
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

			// RTVをクリア
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// 深度バッファをクリア
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// Viewportを設定
			commandList->RSSetViewports(1, &viewport);

			// Scissorを設定
			commandList->RSSetScissorRects(1, &scissorRect);

			// RootSignatureを設定
			commandList->SetGraphicsRootSignature(rootSignature);

			// DescriptorHeapを配列にまとめる
			ID3D12DescriptorHeap* descriptorHeaps[] = {
				srvDescriptorHeap
			};

			// DescriptorHeapをセット
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// PSOを設定
			commandList->SetPipelineState(graphicsPipelineState);

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

			// 使うTextureを設定
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

			// OBJモデルを描画する
			// LoadObjFileで面を頂点列へ展開済みなのでDrawInstancedを使う
			commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

			// フラグが変わってもspriteを変えない
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// ---------------------
			// Sprite描画
			// ---------------------

			// Sprite用VBVを設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);

			// Sprite用IBVを設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite);

			// SpriteはLightingを行わないMaterialを使用する
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());

			// Sprite用TransformationMatrixを設定
			commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

			// Spriteでは常にuvCheckerを使用する
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// Indexを使ってSpriteを描画する
			// Index数6、Instance数1
			//commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

#ifdef USE_IMGUI

			// ImGuiの描画を行う
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

#endif

			// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
			// // 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// コマンドリストの内容を確定させる
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = {
				commandList
			};

			commandQueue->ExecuteCommandLists(1, commandLists);

			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			// Fenceの値を更新
			fenceValue++;

			// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence, fenceValue);

			// Fenceの値が指定したSignal値にたどり着いているか確認する
			// GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue)
			{
				// 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);

				// イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));

			hr = commandList->Reset(
				commandAllocator,
				nullptr
			);

			assert(SUCCEEDED(hr));
		}
	}

	// ------------------------------
	// 解放処理
	// ------------------------------

#ifdef USE_IMGUI

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

#endif

	intermediateResource->Release();
	intermediateResource2->Release();
	wvpResource->Release();
	vertexResource->Release();
	textureResource->Release();
	textureResource2->Release();
	depthStencilResource->Release();
	graphicsPipelineState->Release();
	signatureBlob->Release();
	if (errorBlob)
	{
		errorBlob->Release();
	}
	rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();
	materialResource->Release();
	vertexResourceSprite->Release();
	indexResourceSprite->Release();
	transformationMatrixResourceSprite->Release();
	materialResourceSprite->Release();
	directionalLightResource->Release();

	CloseHandle(fenceEvent);
	fence->Release();
	srvDescriptorHeap->Release();
	rtvDescriptorHeap->Release();
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();
	commandList->Release();
	commandAllocator->Release();
	commandQueue->Release();
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();
	dsvDescriptorHeap->Release();

#ifdef _DEBUG
	debugController->Release();
#endif

	CloseWindow(hwnd);

	// リソースリークチェック
	IDXGIDebug1* debug = nullptr;

	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	// COMの終了
	CoUninitialize();

	return 0;
}