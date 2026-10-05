#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cstdint>

class Input
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update();

	// ------------------------------
	// キーボード
	// ------------------------------

	// キーを押している
	bool PushKey(uint8_t keyNumber);

	// キーを離している
	bool ReleaseKey(uint8_t keyNumber);

	// キーを押した瞬間
	bool TriggerKey(uint8_t keyNumber);

	// キーを離した瞬間
	bool ExitKey(uint8_t keyNumber);

	// ------------------------------
	// マウス
	// ------------------------------

	/// <summary>
	/// マウスボタンを押しているか
	/// 0 = 左
	/// 1 = 右
	/// 2 = 中央
	/// </summary>
	bool PushMouse(uint8_t buttonNumber);

	/// <summary>
	/// マウスのX移動量
	/// </summary>
	LONG GetMouseMoveX() const
	{
		return mouseState_.lX;
	}

	/// <summary>
	/// マウスのY移動量
	/// </summary>
	LONG GetMouseMoveY() const
	{
		return mouseState_.lY;
	}

	/// <summary>
	/// ホイール移動量
	/// </summary>
	LONG GetWheel() const
	{
		return mouseState_.lZ;
	}

private:
	// DirectInput
	Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

	// キーボード
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

	// マウス
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;

	// 現在のキー
	BYTE key_[256] = {};

	// 前フレームのキー
	BYTE preKey_[256] = {};

	// マウス状態
	DIMOUSESTATE2 mouseState_{};
};