#include "D3DResourceLeakChecker.h"

#include <Windows.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#pragma comment(lib, "dxguid.lib")

// =====================================
// デストラクタ
// =====================================
D3DResourceLeakChecker::~D3DResourceLeakChecker() {

#ifdef _DEBUG

    // DXGIデバッグインターフェース
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

    // デバッグインターフェースを取得
    HRESULT hr = DXGIGetDebugInterface1(
        0,
        IID_PPV_ARGS(debug.GetAddressOf())
    );

    // 取得できなかった場合
    if (FAILED(hr)) {
        OutputDebugStringA(
            "DXGI Debug Interfaceの取得に失敗しました。\n"
        );
        return;
    }

    // リソースリークを確認
    debug->ReportLiveObjects(
        DXGI_DEBUG_ALL,
        DXGI_DEBUG_RLO_ALL
    );

#endif
}