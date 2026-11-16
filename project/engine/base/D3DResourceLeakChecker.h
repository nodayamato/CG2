#pragma once

// DirectX12リソースのリークを終了時に確認するクラス
class D3DResourceLeakChecker {
public:
    // デストラクタでリソースリークを確認
    ~D3DResourceLeakChecker();
};