#include "Audio.h"
#include <cassert>
#include <cstring>
#include <fstream>

void Audio::Initialize()
{
	// すでに初期化済みなら何もしない
	if (isInitialized_)
	{
		return;
	}

	// XAudio2エンジンを生成
	HRESULT hr = XAudio2Create(
		xAudio2_.GetAddressOf(),
		0,
		XAUDIO2_DEFAULT_PROCESSOR);

	assert(SUCCEEDED(hr));

	// マスターボイスを生成
	hr = xAudio2_->CreateMasteringVoice(
		&masterVoice_);

	assert(SUCCEEDED(hr));

	isInitialized_ = true;
}

void Audio::LoadWave(const std::string& filename)
{
	// 初期化されていない状態では読み込まない
	assert(isInitialized_);

	// すでに音声を読み込んでいる場合は解放する
	UnloadWave();

	// WAVファイルをバイナリモードで開く
	std::ifstream file(
		filename,
		std::ios_base::binary);

	assert(file.is_open());

	// ------------------------------
	// RIFFヘッダーの読み込み
	// ------------------------------

	RiffHeader riff{};

	file.read(
		reinterpret_cast<char*>(&riff),
		sizeof(riff));

	assert(file.good());

	// ファイルがRIFF形式か確認
	assert(
		std::memcmp(
			riff.chunk.id,
			"RIFF",
			4) == 0);

	// ファイルがWAVE形式か確認
	assert(
		std::memcmp(
			riff.type,
			"WAVE",
			4) == 0);

	// ------------------------------
	// Formatチャンクの読み込み
	// ------------------------------

	FormatChunk format{};

	// Formatチャンクのヘッダーを読み込む
	file.read(
		reinterpret_cast<char*>(&format.chunk),
		sizeof(ChunkHeader));

	assert(file.good());

	// fmtチャンクであることを確認
	assert(
		std::memcmp(
			format.chunk.id,
			"fmt ",
			4) == 0);

	// 読み込めるサイズであることを確認
	assert(
		format.chunk.size <=
		static_cast<int32_t>(sizeof(format.fmt)));

	// Formatチャンク本体を読み込む
	file.read(
		reinterpret_cast<char*>(&format.fmt),
		format.chunk.size);

	assert(file.good());

	// ------------------------------
	// Dataチャンクの読み込み
	// ------------------------------

	ChunkHeader data{};

	// dataチャンクが見つかるまで読み進める
	while (file.read(
		reinterpret_cast<char*>(&data),
		sizeof(data)))
	{
		// dataチャンクを発見したら終了
		if (std::memcmp(data.id, "data", 4) == 0)
		{
			break;
		}

		// JUNKなどの不要なチャンク本体を読み飛ばす
		file.seekg(
			data.size,
			std::ios_base::cur);
	}

	// dataチャンクが見つかったことを確認
	assert(
		std::memcmp(
			data.id,
			"data",
			4) == 0);

	assert(data.size > 0);

	// 音声データ用メモリを確保
	soundData_.pBuffer =
		new BYTE[data.size];

	// 音声データを読み込む
	file.read(
		reinterpret_cast<char*>(
			soundData_.pBuffer),
		data.size);

	assert(file.good());

	// 読み込んだデータを保存
	soundData_.wfex = format.fmt;
	soundData_.bufferSize =
		static_cast<unsigned int>(data.size);

	// ファイルを閉じる
	file.close();
}

void Audio::PlayWave()
{
	// 初期化されていることを確認
	assert(isInitialized_);

	// 音声データが読み込まれていることを確認
	assert(soundData_.pBuffer != nullptr);
	assert(soundData_.bufferSize > 0);

	// 前回のSourceVoiceが残っている場合は破棄
	if (sourceVoice_ != nullptr)
	{
		sourceVoice_->Stop();
		sourceVoice_->DestroyVoice();
		sourceVoice_ = nullptr;
	}

	// 波形フォーマットを元にSourceVoiceを生成
	HRESULT hr =
		xAudio2_->CreateSourceVoice(
			&sourceVoice_,
			&soundData_.wfex);

	assert(SUCCEEDED(hr));

	// 再生する波形データを設定
	XAUDIO2_BUFFER buffer{};

	buffer.pAudioData =
		soundData_.pBuffer;

	buffer.AudioBytes =
		soundData_.bufferSize;

	buffer.Flags =
		XAUDIO2_END_OF_STREAM;

	// 音声データをSourceVoiceへ送る
	hr = sourceVoice_->SubmitSourceBuffer(
		&buffer);

	assert(SUCCEEDED(hr));

	// 再生開始
	hr = sourceVoice_->Start();

	assert(SUCCEEDED(hr));
}

void Audio::UnloadWave()
{
	// 音声データを解放
	delete[] soundData_.pBuffer;

	soundData_.pBuffer = nullptr;
	soundData_.bufferSize = 0;
	soundData_.wfex = {};
}

void Audio::Finalize()
{
	// SourceVoiceを破棄
	if (sourceVoice_ != nullptr)
	{
		sourceVoice_->Stop();
		sourceVoice_->DestroyVoice();
		sourceVoice_ = nullptr;
	}

	// MasteringVoiceを破棄
	if (masterVoice_ != nullptr)
	{
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}

	// XAudio2本体を解放
	xAudio2_.Reset();

	// XAudio2を止めた後で音声データを解放
	UnloadWave();

	isInitialized_ = false;
}

Audio::~Audio()
{
	Finalize();
}