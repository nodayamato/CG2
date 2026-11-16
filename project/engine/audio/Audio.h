#pragma once

#include <Windows.h>
#include <xaudio2.h>
#include <wrl.h>

#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

// =====================================
// 音声管理クラス
// =====================================
class Audio {
public:

    // 初期化
    void Initialize();

    // WAV / MP3などを読み込む
    void LoadSound(const std::string& filename);

    // 従来の呼び出し方法も残す
    void LoadWave(const std::string& filename);

    // 読み込んだ音声を再生
    void PlayWave();

    // 終了処理
    void Finalize();

    // デストラクタ
    ~Audio();

private:

    // =================================
    // 音声データ
    // =================================
    struct SoundData {

        // WAVEFORMATEX形式の情報
        // 拡張フォーマットにも対応するため
        // バイト配列で保持する
        std::vector<BYTE> waveFormat;

        // デコード済みPCMデータ
        std::vector<BYTE> pcmData;

        // 読み込んだファイル
        std::string filename;
    };

    // 音声データを解放
    void UnloadWave();

    // 再生中のSourceVoiceを破棄
    void DestroySourceVoice();

private:

    // XAudio2本体
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;

    // 最終出力先
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    // 再生用Voice
    IXAudio2SourceVoice* sourceVoice_ = nullptr;

    // 読み込んだ音声データ
    SoundData soundData_{};

    // 初期化済みか
    bool isInitialized_ = false;

    // Media Foundation初期化済みか
    bool isMediaFoundationInitialized_ = false;
};