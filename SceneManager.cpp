#include "SceneManager.h"

#include <cassert>
#include <utility>

// =====================================
// シーンの登録
// =====================================
void SceneManager::RegisterScene(
    SceneType type,
    std::unique_ptr<BaseScene> scene
) {
    const auto index =
        static_cast<std::size_t>(type);

    assert(index < scenes_.size());
    assert(scene != nullptr);
    assert(scenes_[index] == nullptr);

    scene->SetSceneManager(this);

    scenes_[index] = std::move(scene);
}

// =====================================
// 最初のシーンを設定
// =====================================
void SceneManager::Initialize(
    SceneType firstScene
) {
    const auto index =
        static_cast<std::size_t>(firstScene);

    assert(index < scenes_.size());
    assert(scenes_[index] != nullptr);

    currentType_ = firstScene;
    currentScene_ = scenes_[index].get();

    hasNextScene_ = false;

    currentScene_->Initialize();
}

// =====================================
// シーンの切り替えを予約
// =====================================
void SceneManager::ChangeScene(
    SceneType nextScene
) {
    const auto index =
        static_cast<std::size_t>(nextScene);

    assert(index < scenes_.size());
    assert(scenes_[index] != nullptr);

    if (nextScene == currentType_) {
        return;
    }

    nextType_ = nextScene;
    hasNextScene_ = true;
}

// =====================================
// 更新
// =====================================
void SceneManager::Update() {

    // 切り替え要求がある場合
    if (hasNextScene_) {

        // 今のシーンを終了
        currentScene_->Finalize();

        // 新しいシーンへ切り替える
        currentType_ = nextType_;

        currentScene_ =
            scenes_[
                static_cast<std::size_t>(
                    currentType_
                    )
            ].get();

        hasNextScene_ = false;

        // 新しいシーンを初期化
        currentScene_->Initialize();
    }

    // 現在のシーンだけ更新
    if (currentScene_ != nullptr) {
        currentScene_->Update();
    }
}

// =====================================
// 描画
// =====================================
void SceneManager::Draw() {
    if (currentScene_ != nullptr) {
        currentScene_->Draw();
    }
}

// =====================================
// 終了処理
// =====================================
void SceneManager::Finalize() {

    if (currentScene_ != nullptr) {
        currentScene_->Finalize();
        currentScene_ = nullptr;
    }

    for (auto& scene : scenes_) {
        scene.reset();
    }

    hasNextScene_ = false;
}