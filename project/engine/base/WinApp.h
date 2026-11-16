#pragma once

#include <Windows.h>

// Windowsアプリケーション管理クラス
class WinApp {
public:

    // クライアント領域のサイズ
    static constexpr int kClientWidth = 1280;
    static constexpr int kClientHeight = 720;

    // 初期化
    void Initialize();

    // メッセージ処理
    bool ProcessMessage();

    // 終了要求を取得
    bool IsEndRequested() const {
        return isEndRequested_;
    }

    // 終了処理
    void Finalize();

    // ウィンドウハンドルのgetter
    HWND GetHwnd() const {
        return hwnd_;
    }

    // インスタンスハンドルのgetter
    HINSTANCE GetHInstance() const {
        return hInstance_;
    }

private:

    // ウィンドウプロシージャ
    // 静的メンバ関数
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wparam,
        LPARAM lparam
    );

    // ウィンドウハンドル
    HWND hwnd_ = nullptr;

    // インスタンスハンドル
    HINSTANCE hInstance_ = nullptr;

    // 終了要求
    bool isEndRequested_ = false;

    // ウィンドウクラス名
    static constexpr const wchar_t* kWindowClassName = L"CG2WindowClass";
};
