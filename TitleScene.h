#pragma once

#include "BaseScene.h"

class Input;

// =====================================
// タイトルシーン
// =====================================
class TitleScene final : public BaseScene {
public:
    explicit TitleScene(Input* input);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // InputはGameが所有
    Input* input_ = nullptr;
};