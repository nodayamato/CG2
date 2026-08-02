#pragma warning(push)
// C4023の警告を無効化する
#pragma warning(disable:4023)
#include <Windows.h>
#include "ConvertString.h"
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
#include <cassert>
// デバッグ用のライブラリ
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#pragma warning(pop)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
	WPARAM wparam, LPARAM lparam) {

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

// Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

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

	// ディスクリプタヒープを生成
	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};

	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用

	rtvDescriptorHeapDesc.NumDescriptors = 2; // ダブルバッファ用に2つ。多くても別に構わない

	hr = device->CreateDescriptorHeap(
		&rtvDescriptorHeapDesc,
		IID_PPV_ARGS(&rtvDescriptorHeap)
	);

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
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

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

	// ------------------------------
	// テキストログ出力のテスト
	// ------------------------------

	Log(logStream, "Hello\n");
	Log(logStream, "PlayerHP : " + std::to_string(100));

	// ------------------------------
	// ゲームループ
	// ------------------------------

	MSG msg{};
	// ウィンドウのXボタンが押されるまでループする
	while (msg.message != WM_QUIT) {
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

			// 描画先のRTVを設定する
			commandList->OMSetRenderTargets(
				1,
				&rtvHandles[backBufferIndex],
				false,
				nullptr
			);

			// 指定した色で画面全体をクリアする
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(
				rtvHandles[backBufferIndex],
				clearColor,
				0,
				nullptr
			);

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

	CloseHandle(fenceEvent);
	fence->Release();
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

	return 0;
}