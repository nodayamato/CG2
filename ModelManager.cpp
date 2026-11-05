#include "ModelManager.h"

#include "DirectXCommon.h"
#include "TextureManager.h"

#include <cassert>
#include <utility>

// =====================================
// 初期化
// =====================================
void ModelManager::Initialize(
    DirectXCommon* dxCommon,
    TextureManager* textureManager
) {
    assert(dxCommon != nullptr);
    assert(textureManager != nullptr);
    assert(models_.empty());

    dxCommon_ = dxCommon;
    textureManager_ = textureManager;
}

// =====================================
// モデル検索用キー作成
// =====================================
std::string ModelManager::MakeModelKey(
    const std::string& directoryPath,
    const std::string& filename
) const {

    // ディレクトリとファイル名を連結
    return directoryPath + "/" + filename;
}

// =====================================
// モデル読み込み
// =====================================
Model* ModelManager::LoadModel(
    const std::string& directoryPath,
    const std::string& filename
) {
    assert(dxCommon_ != nullptr);
    assert(textureManager_ != nullptr);
    assert(!filename.empty());

    // ---------------------------------
    // モデルを検索
    // ---------------------------------
    std::string modelKey =
        MakeModelKey(directoryPath, filename);

    auto it = models_.find(modelKey);

    // 読み込み済みなら再利用する
    if (it != models_.end()) {
        return it->second.get();
    }

    // ---------------------------------
    // 新しいモデルを作成
    // ---------------------------------
    std::unique_ptr<Model> model =
        std::make_unique<Model>();

    // ModelDataの読み込みとGPUリソース生成
    model->Initialize(
        dxCommon_,
        textureManager_,
        directoryPath,
        filename
    );

    // Modelのポインタを取得
    Model* modelPtr = model.get();

    // ---------------------------------
    // 管理リストに登録
    // ---------------------------------
    models_.emplace(
        modelKey,
        std::move(model)
    );

    return modelPtr;
}

// =====================================
// 生成モデルの登録
// =====================================
Model* ModelManager::RegisterGeneratedModel(
    const std::string& modelName,
    ModelData modelData,
    uint32_t textureIndex
) {
    assert(dxCommon_ != nullptr);
    assert(textureManager_ != nullptr);
    assert(!modelName.empty());
    assert(!modelData.vertices.empty());

    // ファイルモデルとの名前衝突を防ぐ
    std::string modelKey =
        "generated:" + modelName;

    // すでに登録済みなら再利用
    auto it = models_.find(modelKey);

    if (it != models_.end()) {
        return it->second.get();
    }

    // 新しいModelを生成
    std::unique_ptr<Model> model =
        std::make_unique<Model>();

    model->InitializeFromData(
        dxCommon_,
        textureManager_,
        std::move(modelData),
        textureIndex
    );

    Model* modelPtr = model.get();

    // 一括管理に登録
    models_.emplace(
        modelKey,
        std::move(model)
    );

    return modelPtr;
}

// =====================================
// 読み込み済みモデル取得
// =====================================
Model* ModelManager::FindModel(
    const std::string& directoryPath,
    const std::string& filename
) const {

    std::string modelKey =
        MakeModelKey(directoryPath, filename);

    auto it = models_.find(modelKey);

    // 見つからなければnullptr
    if (it == models_.end()) {
        return nullptr;
    }

    // 登録済みのModelを返す
    return it->second.get();
}

// =====================================
// 終了処理
// =====================================
void ModelManager::Finalize() {

    // ---------------------------------
    // 各Modelのリソースを解放
    // ---------------------------------
    for (auto& pair : models_) {

        if (pair.second != nullptr) {
            pair.second->Finalize();
        }
    }

    // Modelのインスタンスを破棄
    models_.clear();

    // 外部クラスへの参照を解除
    dxCommon_ = nullptr;
    textureManager_ = nullptr;
}