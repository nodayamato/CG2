#pragma once

#include "Math.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class DirectXCommon;
class SrvManager;

// =====================================
// テクスチャ管理クラス
// =====================================
class TextureManager {
public:

    // 初期化
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    // テクスチャ読み込み
    // 読み込み済みなら既存のSRV番号を返す
    uint32_t LoadTexture(
        const std::string& filePath
    );

    // GPUハンドル取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(
        uint32_t textureIndex
    ) const;

    // テクスチャサイズ取得
    Vector2 GetTextureSize(
        uint32_t textureIndex
    ) const;

    // GPU転送完了後に中間リソースを解放
    void ReleaseUploadResources();

    // 終了処理
    void Finalize();

private:

    // テクスチャ1枚分の情報
    struct TextureData {

        // ファイルパス
        std::string filePath;

        // SrvManagerから取得した番号
        uint32_t srvIndex = 0;

        // テクスチャ本体
        Microsoft::WRL::ComPtr<ID3D12Resource>
            resource;

        // GPU転送用リソース
        Microsoft::WRL::ComPtr<ID3D12Resource>
            uploadResource;

        // GPUハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE
            gpuHandle{};

        // 画像サイズ
        Vector2 textureSize{
            0.0f, 0.0f
        };
    };

    // SRV番号に対応する配列位置を取得
    size_t GetArrayIndex(
        uint32_t textureIndex
    ) const;

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // SrvManager（所有しない）
    SrvManager* srvManager_ = nullptr;

    // テクスチャ一覧
    std::vector<TextureData> textures_;

    // ファイルパス → SRV番号
    std::unordered_map<
        std::string,
        uint32_t
    > textureIndices_;

    // SRV番号 → 配列位置
    std::unordered_map<
        uint32_t,
        size_t
    > textureArrayIndices_;
};