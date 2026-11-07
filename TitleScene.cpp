#include "TitleScene.h"
#include "SceneManager.h"
#include "Input.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// =====================================
// コンストラクタ
// =====================================
TitleScene::TitleScene(Input* input)
    : input_(input) {}

// =====================================
// 初期化
// =====================================
void TitleScene::Initialize() {}

// =====================================
// 更新
// =====================================
void TitleScene::Update() {

    // Enterキーで開始
    bool start = input_->PushKey(DIK_RETURN);

#ifdef USE_IMGUI

    ImGui::Begin("Title Scene");

    ImGui::Text("TITLE SCENE");
    ImGui::Text("Press ENTER to start");

    // ボタンでも開始できる
    if (ImGui::Button("START GAME")) {
        start = true;
    }

    ImGui::End();

#endif

    // ゲームプレイシーンへ
    if (start) {
        sceneManager_->ChangeScene(
            SceneType::GamePlay
        );
    }
}

// =====================================
// 描画
// =====================================
void TitleScene::Draw() {

    // ImGuiはUpdate側で内容を構築し、
    // Game側のImGuiManagerで描画する
}

// =====================================
// 終了
// =====================================
void TitleScene::Finalize() {}