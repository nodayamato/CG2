#pragma once

#include "Model.h"

#include <memory>
#include <string>
#include <unordered_map>

class DirectXCommon;
class TextureManager;

// =====================================
// モデル一括管理クラス
// =====================================
class ModelManager {
public:

    // 初期化
    void Initialize(
        DirectXCommon* dxCommon,
        TextureManager* textureManager
    );

    // モデル読み込み
    // 読み込み済みなら既存のModelを返す
    Model* LoadModel(
        const std::string& directoryPath,
        const std::string& filename
    );

    // 生成したモデルを登録
    Model* RegisterGeneratedModel(
        const std::string& modelName,
        ModelData modelData,
        uint32_t textureIndex
    );

    // 読み込み済みモデルを取得
    // 見つからない場合はnullptr
    Model* FindModel(
        const std::string& directoryPath,
        const std::string& filename
    ) const;

    // 終了処理
    void Finalize();

private:

    // モデル検索用のキーを作成
    std::string MakeModelKey(
        const std::string& directoryPath,
        const std::string& filename
    ) const;

private:

    // DirectXCommon（所有しない）
    DirectXCommon* dxCommon_ = nullptr;

    // TextureManager（所有しない）
    TextureManager* textureManager_ = nullptr;

    // ModelManagerがModelを所有する
    std::unordered_map<
        std::string,
        std::unique_ptr<Model>
    > models_;
};