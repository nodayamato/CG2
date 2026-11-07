#pragma once

#include "BaseScene.h"

#include <functional>

class Input;

// =====================================
// ゲームプレイシーン
// =====================================
class GamePlayScene final : public BaseScene {
public:
    GamePlayScene(
        Input* input,
        std::function<void()> updateGameplay,
        std::function<void()> drawGameplay
    );

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    Input* input_ = nullptr;

    std::function<void()> updateGameplay_;
    std::function<void()> drawGameplay_;
};