#pragma once

#include <memory>

// =====================================
// ゲーム管理クラス
// =====================================
class Game {
public:

    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // 初期化
    void Initialize();

    // 更新
    void Update();

    // 描画
    void Draw();

    // 終了処理
    void Finalize();

    // ゲーム終了判定
    bool IsEndRequested() const;

private:

    // ゲームの内部データ
    struct Impl;
    std::unique_ptr<Impl> impl_;
};