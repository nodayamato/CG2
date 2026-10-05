#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT hr;

	// ------------------------------
	// DirectInput初期化
	// ------------------------------

	hr = DirectInput8Create(
		hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(
			directInput_.GetAddressOf()),
		nullptr);

	assert(SUCCEEDED(hr));

	// ------------------------------
	// キーボード
	// ------------------------------

	// キーボードデバイスを作成
	hr = directInput_->CreateDevice(
		GUID_SysKeyboard,
		keyboard_.GetAddressOf(),
		nullptr);

	assert(SUCCEEDED(hr));

	// キーボード入力形式
	hr = keyboard_->SetDataFormat(
		&c_dfDIKeyboard);

	assert(SUCCEEDED(hr));

	// キーボードの排他制御
	hr = keyboard_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND |
		DISCL_NONEXCLUSIVE |
		DISCL_NOWINKEY);

	assert(SUCCEEDED(hr));

	// ------------------------------
	// マウス
	// ------------------------------

	// マウスデバイスを作成
	hr = directInput_->CreateDevice(
		GUID_SysMouse,
		mouse_.GetAddressOf(),
		nullptr);

	assert(SUCCEEDED(hr));

	// マウス入力形式
	hr = mouse_->SetDataFormat(
		&c_dfDIMouse2);

	assert(SUCCEEDED(hr));

	// マウスの排他制御
	hr = mouse_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND |
		DISCL_NONEXCLUSIVE);

	assert(SUCCEEDED(hr));
}

void Input::Update()
{
	// ------------------------------
	// キーボード更新
	// ------------------------------

	// 前フレームを保存
	memcpy(
		preKey_,
		key_,
		sizeof(key_));

	// キーボード取得開始
	keyboard_->Acquire();

	// キーボード状態を取得
	HRESULT hr =
		keyboard_->GetDeviceState(
			sizeof(key_),
			key_);

	// 取得に失敗したら再取得
	if (FAILED(hr))
	{
		keyboard_->Acquire();

		keyboard_->GetDeviceState(
			sizeof(key_),
			key_);
	}

	// ------------------------------
	// マウス更新
	// ------------------------------

	// 毎フレームリセット
	mouseState_ = {};

	// マウス取得開始
	mouse_->Acquire();

	// マウス状態を取得
	hr = mouse_->GetDeviceState(
		sizeof(DIMOUSESTATE2),
		&mouseState_);

	// 取得に失敗したら再取得
	if (FAILED(hr))
	{
		mouse_->Acquire();

		mouse_->GetDeviceState(
			sizeof(DIMOUSESTATE2),
			&mouseState_);
	}
}

bool Input::PushKey(uint8_t keyNumber)
{
	return
		(key_[keyNumber] & 0x80) != 0;
}

bool Input::ReleaseKey(uint8_t keyNumber)
{
	return
		(key_[keyNumber] & 0x80) == 0;
}

bool Input::TriggerKey(uint8_t keyNumber)
{
	return
		(key_[keyNumber] & 0x80) != 0 &&
		(preKey_[keyNumber] & 0x80) == 0;
}

bool Input::ExitKey(uint8_t keyNumber)
{
	return
		(key_[keyNumber] & 0x80) == 0 &&
		(preKey_[keyNumber] & 0x80) != 0;
}

bool Input::PushMouse(uint8_t buttonNumber)
{
	if (buttonNumber >= 8)
	{
		return false;
	}

	return
		(mouseState_.rgbButtons[buttonNumber] & 0x80) != 0;
}