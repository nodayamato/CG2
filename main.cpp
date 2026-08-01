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
#pragma warning(pop)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

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



	// デバッグ出力
    Log(logStream, "Hello\n");
    Log(logStream, "PlayerHP : " + std::to_string(100));

	MSG msg{};
	// ウィンドウのXボタンが押されるまでループする
	while (msg.message != WM_QUIT) {
		// Windowにメッセージが来ていたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
        } else {
			// ゲームの処理
        }
	}

	return 0;
}