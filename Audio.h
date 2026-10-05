#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <wrl.h>
#include <cstdint>
#include <string>

#pragma comment(lib, "xaudio2.lib")

/// <summary>
/// 音声管理クラス
/// </summary>
class Audio
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// WAVファイルを読み込む
	/// </summary>
	/// <param name="filename">WAVファイルのパス</param>
	void LoadWave(const std::string& filename);

	/// <summary>
	/// 読み込んだ音声を再生する
	/// </summary>
	void PlayWave();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~Audio();

private:
	/// <summary>
	/// チャンクヘッダ
	/// </summary>
	struct ChunkHeader
	{
		char id[4];
		int32_t size;
	};

	/// <summary>
	/// RIFFヘッダ
	/// </summary>
	struct RiffHeader
	{
		ChunkHeader chunk;
		char type[4];
	};

	/// <summary>
	/// Formatチャンク
	/// </summary>
	struct FormatChunk
	{
		ChunkHeader chunk;
		WAVEFORMATEX fmt;
	};

	/// <summary>
	/// 音声データ
	/// </summary>
	struct SoundData
	{
		// 波形フォーマット
		WAVEFORMATEX wfex{};

		// 音声データの先頭アドレス
		BYTE* pBuffer = nullptr;

		// 音声データのサイズ
		unsigned int bufferSize = 0;
	};

	/// <summary>
	/// 音声データを解放する
	/// </summary>
	void UnloadWave();

private:
	// XAudio2本体
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;

	// 全ての音声の最終出力先
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// 再生用SourceVoice
	IXAudio2SourceVoice* sourceVoice_ = nullptr;

	// 読み込んだ音声データ
	SoundData soundData_{};

	// 初期化済みかどうか
	bool isInitialized_ = false;
};