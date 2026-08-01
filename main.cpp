#pragma warning(push)
// C4023の警告を無効化する
#pragma warning(disable:4023)
#include <Windows.h>
#include <cstdint>
#pragma warning(pop)

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

// Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

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

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	return 0;
}