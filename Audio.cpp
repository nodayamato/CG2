#include "Audio.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <cstring>
#include <limits>
#include <stdexcept>

// Media Foundationのライブラリ
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {

    // HRESULTのエラー確認
    void CheckHR(HRESULT hr) {

        if (FAILED(hr)) {
            throw std::runtime_error(
                "Audio processing failed."
            );
        }
    }

    // ファイルパスをwstringへ変換
    std::wstring ConvertPath(
        const std::string& path
    ) {
        if (path.empty()) {
            throw std::runtime_error(
                "Audio file path is empty."
            );
        }

        // まずUTF-8として解釈する
        UINT codePage = CP_UTF8;

        int length = MultiByteToWideChar(
            codePage, MB_ERR_INVALID_CHARS,
            path.c_str(), -1, nullptr, 0
        );

        // UTF-8でなければ従来の文字コード
        if (length == 0) {
            codePage = CP_ACP;
            length = MultiByteToWideChar(
                codePage, 0,
                path.c_str(), -1, nullptr, 0
            );
        }

        if (length == 0) {
            throw std::runtime_error(
                "Cannot convert audio path."
            );
        }

        std::wstring result(
            static_cast<size_t>(length),
            L'\0'
        );

        MultiByteToWideChar(
            codePage, 0,
            path.c_str(), -1,
            result.data(), length
        );

        result.pop_back();
        return result;
    }

} // namespace

// =====================================
// 初期化
// =====================================
void Audio::Initialize() {

    if (isInitialized_) {
        return;
    }

    // Media Foundation初期化
    CheckHR(
        MFStartup(MF_VERSION)
    );

    isMediaFoundationInitialized_ = true;

    // XAudio2初期化
    CheckHR(
        XAudio2Create(
            xAudio2_.GetAddressOf(),
            0,
            XAUDIO2_DEFAULT_PROCESSOR
        )
    );

    // マスターボイス生成
    CheckHR(
        xAudio2_->CreateMasteringVoice(
            &masterVoice_
        )
    );

    isInitialized_ = true;
}

// =====================================
// 音声ファイル読み込み
// WAV / MP3などに対応
// =====================================
void Audio::LoadSound(
    const std::string& filename
) {
    if (!isInitialized_) {
        throw std::runtime_error(
            "Audio is not initialized."
        );
    }

    // 同じファイルは読み込み直さない
    if (soundData_.filename == filename &&
        !soundData_.pcmData.empty()) {
        return;
    }

    // 前回の再生を終了してからデータ解放
    UnloadWave();

    std::wstring widePath =
        ConvertPath(filename);

    // ---------------------------------
    // SourceReader生成
    // ---------------------------------
    Microsoft::WRL::ComPtr<IMFSourceReader>
        reader;

    CheckHR(
        MFCreateSourceReaderFromURL(
            widePath.c_str(),
            nullptr,
            reader.GetAddressOf()
        )
    );

    // 音声ストリームを選択
    CheckHR(
        reader->SetStreamSelection(
            MF_SOURCE_READER_ALL_STREAMS,
            FALSE
        )
    );

    CheckHR(
        reader->SetStreamSelection(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            TRUE
        )
    );

    // ---------------------------------
    // PCMへ変換するよう指定
    // ---------------------------------
    Microsoft::WRL::ComPtr<IMFMediaType>
        mediaType;

    CheckHR(
        MFCreateMediaType(
            mediaType.GetAddressOf()
        )
    );

    CheckHR(
        mediaType->SetGUID(
            MF_MT_MAJOR_TYPE,
            MFMediaType_Audio
        )
    );

    CheckHR(
        mediaType->SetGUID(
            MF_MT_SUBTYPE,
            MFAudioFormat_PCM
        )
    );

    CheckHR(
        reader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            nullptr,
            mediaType.Get()
        )
    );

    // 実際に設定されたPCMフォーマット
    Microsoft::WRL::ComPtr<IMFMediaType>
        currentMediaType;

    CheckHR(
        reader->GetCurrentMediaType(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            currentMediaType.GetAddressOf()
        )
    );

    // ---------------------------------
    // WAVEFORMATEXへ変換
    // ---------------------------------
    WAVEFORMATEX* waveFormat = nullptr;
    UINT32 waveFormatSize = 0;

    CheckHR(
        MFCreateWaveFormatExFromMFMediaType(
            currentMediaType.Get(),
            &waveFormat,
            &waveFormatSize,
            MFWaveFormatExConvertFlag_Normal
        )
    );

    soundData_.waveFormat.assign(
        reinterpret_cast<BYTE*>(waveFormat),
        reinterpret_cast<BYTE*>(waveFormat) +
        waveFormatSize
    );

    CoTaskMemFree(waveFormat);
    waveFormat = nullptr;

    // ---------------------------------
    // サンプルを最後まで読み取る
    // ---------------------------------
    while (true) {

        DWORD streamFlags = 0;

        Microsoft::WRL::ComPtr<IMFSample>
            sample;

        CheckHR(
            reader->ReadSample(
                MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                0,
                nullptr,
                &streamFlags,
                nullptr,
                sample.GetAddressOf()
            )
        );

        // ストリームエラー
        if (streamFlags &
            MF_SOURCE_READERF_ERROR) {
            throw std::runtime_error(
                "Audio stream error."
            );
        }

        // フォーマット変更は今回未対応
        if (streamFlags &
            MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            throw std::runtime_error(
                "Audio format changed."
            );
        }

        if (sample != nullptr) {

            Microsoft::WRL::ComPtr<IMFMediaBuffer>
                buffer;

            CheckHR(
                sample->ConvertToContiguousBuffer(
                    buffer.GetAddressOf()
                )
            );

            BYTE* data = nullptr;
            DWORD maxLength = 0;
            DWORD currentLength = 0;

            CheckHR(
                buffer->Lock(
                    &data,
                    &maxLength,
                    &currentLength
                )
            );

            // PCMデータをメモリに保存
            soundData_.pcmData.insert(
                soundData_.pcmData.end(),
                data,
                data + currentLength
            );

            CheckHR(
                buffer->Unlock()
            );
        }

        // ファイルの末尾
        if (streamFlags &
            MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
    }

    if (soundData_.pcmData.empty()) {
        throw std::runtime_error(
            "Audio data is empty."
        );
    }

    // XAudio2のバッファサイズ上限
    if (soundData_.pcmData.size() >
        (std::numeric_limits<UINT32>::max)()) {

        throw std::runtime_error(
            "Audio data is too large."
        );
    }

    // 読み込み成功したパスを保存
    soundData_.filename = filename;
}

// =====================================
// 以前のLoadWaveとの互換性
// =====================================
void Audio::LoadWave(
    const std::string& filename
) {
    LoadSound(filename);
}

// =====================================
// 読み込んだ音声を再生
// =====================================
void Audio::PlayWave() {

    if (!isInitialized_ ||
        soundData_.pcmData.empty() ||
        soundData_.waveFormat.empty()) {

        throw std::runtime_error(
            "No audio data loaded."
        );
    }

    // 前回のSourceVoiceを破棄
    // PCMデータは保持する
    DestroySourceVoice();

    // 音声フォーマット
    const WAVEFORMATEX* format =
        reinterpret_cast<const WAVEFORMATEX*>(
            soundData_.waveFormat.data()
            );

    // SourceVoice生成
    CheckHR(
        xAudio2_->CreateSourceVoice(
            &sourceVoice_,
            format
        )
    );

    // 再生用バッファ
    XAUDIO2_BUFFER buffer{};

    buffer.pAudioData =
        soundData_.pcmData.data();

    buffer.AudioBytes =
        static_cast<UINT32>(
            soundData_.pcmData.size()
            );

    buffer.Flags = XAUDIO2_END_OF_STREAM;

    // PCMデータを渡す
    CheckHR(
        sourceVoice_->SubmitSourceBuffer(
            &buffer
        )
    );

    // 再生開始
    CheckHR(
        sourceVoice_->Start()
    );
}

// =====================================
// SourceVoice破棄
// =====================================
void Audio::DestroySourceVoice() {

    if (sourceVoice_ != nullptr) {

        sourceVoice_->Stop();
        sourceVoice_->FlushSourceBuffers();
        sourceVoice_->DestroyVoice();

        sourceVoice_ = nullptr;
    }
}

// =====================================
// 音声データ解放
// =====================================
void Audio::UnloadWave() {

    // 再生側がデータを参照しているので
    // 必ず先に停止させる
    DestroySourceVoice();

    soundData_.pcmData.clear();
    soundData_.waveFormat.clear();
    soundData_.filename.clear();
}

// =====================================
// 終了処理
// =====================================
void Audio::Finalize() {

    // SourceVoice終了
    DestroySourceVoice();

    // 音声データ解放
    UnloadWave();

    // MasteringVoice終了
    if (masterVoice_ != nullptr) {

        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }

    // XAudio2解放
    xAudio2_.Reset();

    // Media Foundation終了
    if (isMediaFoundationInitialized_) {

        MFShutdown();

        isMediaFoundationInitialized_ = false;
    }

    isInitialized_ = false;
}

// =====================================
// デストラクタ
// =====================================
Audio::~Audio() {
    Finalize();
}