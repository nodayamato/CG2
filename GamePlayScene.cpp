#include "GamePlayScene.h"
#include "SceneManager.h"
#include "Input.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <utility>

// =====================================
// コンストラクタ
// =====================================
GamePlayScene::GamePlayScene(
    Input* input,
    std::function<void()> updateGameplay,
    std::function<void()> drawGameplay
)
    : input_(input),
    updateGameplay_(std::move(updateGameplay)),
    drawGameplay_(std::move(drawGameplay)) {}

// =====================================
// 初期化
// =====================================
void GamePlayScene::Initialize() {

    // 既存の描画リソースは
    // Game::Initializeで初期化済み
}

// =====================================
// 更新
// =====================================
void GamePlayScene::Update() {

    // Tキーでタイトルへ
    if (input_->PushKey(DIK_T)) {

        sceneManager_->ChangeScene(
            SceneType::Title
        );

        return;
    }

    // 今までのゲーム更新処理
    updateGameplay_();
}

// =====================================
// 描画
// =====================================
void GamePlayScene::Draw() {

    // 今までのゲーム描画処理
    drawGameplay_();
}

// =====================================
// 終了
// =====================================
void GamePlayScene::Finalize() {

    // リソースの解放は
    // Game::Finalizeでまとめて行う
}