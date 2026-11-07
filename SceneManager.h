#pragma once

#include "BaseScene.h"

#include <array>
#include <cstddef>
#include <memory>

// =====================================
// シーンの種類
// =====================================
enum class SceneType : std::size_t {
    Title = 0,
    GamePlay,
    Count
};

// =====================================
// シーン管理クラス
// =====================================
class SceneManager {
public:
    // シーン登録
    void RegisterScene(
        SceneType type,
        std::unique_ptr<BaseScene> scene
    );

    // 最初のシーンを設定
    void Initialize(SceneType firstScene);

    // シーン切り替え要求
    void ChangeScene(SceneType nextScene);

    // 更新
    void Update();

    // 描画
    void Draw();

    // 終了
    void Finalize();

    SceneType GetCurrentSceneType() const {
        return currentType_;
    }

private:
    static constexpr std::size_t kSceneCount =
        static_cast<std::size_t>(
            SceneType::Count
            );

    // 全シーンを所有する
    std::array<
        std::unique_ptr<BaseScene>,
        kSceneCount
    > scenes_{};

    // 現在のシーン
    BaseScene* currentScene_ = nullptr;

    SceneType currentType_ = SceneType::Title;
    SceneType nextType_ = SceneType::Title;

    bool hasNextScene_ = false;
};