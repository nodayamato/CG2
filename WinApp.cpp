#include "WinApp.h"
#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

// ImGuiのウィンドウプロシージャを宣言
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

#endif

// ウィンドウ初期化
void WinApp::Initialize() {

    // インスタンスハンドルを取得
    hInstance_ = GetModuleHandle(nullptr);

    // 終了要求をリセット
    isEndRequested_ = false;

    assert(hInstance_ != nullptr);

    // ウィンドウサイズ設定
    RECT wrc = {
        0, 0,
        kClientWidth,
        kClientHeight
    };

    BOOL result = AdjustWindowRect(
        &wrc,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    assert(result != FALSE);

    // ウィンドウクラス登録
    WNDCLASS wc{};
	// ウィンドウプロシージャ
    wc.lpfnWndProc = WinApp::WindowProc;
	// ウィンドウクラス名
    wc.lpszClassName = kWindowClassName;
	// インスタンスハンドル
    wc.hInstance = hInstance_;
	// カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// ウィンドウクラスを登録する
    ATOM atom = RegisterClass(&wc);
    assert(atom != 0);

    // ウィンドウ生成
    hwnd_ = CreateWindow(
		kWindowClassName,          // クラス名
		L"CG2",                    // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,       // ウィンドウスタイル
		CW_USEDEFAULT,             // 表示位置X
		CW_USEDEFAULT,             // 表示位置Y
		wrc.right - wrc.left,      // ウィンドウ幅
		wrc.bottom - wrc.top,      // ウィンドウ高
		nullptr,                   // 親ウィンドウハンドル
		nullptr,                   // メニューハンドル
		hInstance_,                // インスタンスハンドル
		nullptr                    // 追加パラメータ
    );

    assert(hwnd_ != nullptr);

    // ウィンドウ表示
    ShowWindow(hwnd_, SW_SHOW);
}

// メッセージ処理
bool WinApp::ProcessMessage() {

    MSG msg{};

    // メッセージがなければfalse
    if (!PeekMessage(
        &msg,
        nullptr,
        0,
        0,
        PM_REMOVE)) {

        return false;
    }

    // 終了メッセージ
    if (msg.message == WM_QUIT) {
        isEndRequested_ = true;
        return true;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);

    return true;
}

// 終了処理
void WinApp::Finalize() {

    // ウィンドウを破棄
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    // 登録済みのウィンドウクラスを解除
    UnregisterClass(
        kWindowClassName,
        hInstance_
    );

    hInstance_ = nullptr;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI

    // ImGuiのWndProcHandlerにメッセージを渡す
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return TRUE;
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