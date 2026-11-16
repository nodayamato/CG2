#pragma once

class SceneManager;

// =====================================
// シーン共通の基底クラス
// =====================================
class BaseScene {
public:
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void Finalize() = 0;

    // シーンマネージャを登録
    void SetSceneManager(SceneManager* manager) {
        sceneManager_ = manager;
    }

protected:
    // 借りて使うだけなのでdeleteしない
    SceneManager* sceneManager_ = nullptr;
};