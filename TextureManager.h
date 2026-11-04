#pragma once

#include "Math.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>

class DirectXCommon;

// =====================================
// テクスチャ管理クラス
// =====================================
class TextureManager {
public:

    // 既存のSRV 0～4番を使用しない
    static constexpr uint32_t kFirstTextureIndex = 5;

    // DirectXCommonのSRVヒープ数と合わせる
    static constexpr uint32_t kMaxSRVCount = 128;

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // テクスチャ読み込み
    // 読み込み済みなら既存の番号を返す
    uint32_t LoadTexture(const std::string& filePath);

    // テクスチャのGPUハンドル取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(
        uint32_t textureIndex
    ) const;

    // テクスチャの画像サイズ取得
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

        // テクスチャ本体
        Microsoft::WRL::ComPtr<ID3D12Resource>
            resource;

        // GPU転送用の中間リソース
        Microsoft::WRL::ComPtr<ID3D12Resource>
            uploadResource;

        // SRVのGPUハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE
            gpuHandle{};

        // 画像サイズ
        Vector2 textureSize{ 0.0f, 0.0f };
    };

    // テクスチャ番号を配列位置へ変換
    size_t GetArrayIndex(
        uint32_t textureIndex
    ) const;

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // 読み込み済みテクスチャ一覧
    std::vector<TextureData> textures_;

    // ファイル名からテクスチャ番号を検索
    std::unordered_map<std::string, uint32_t>
        textureIndices_;
};