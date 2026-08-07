#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cstdint>

class Input
{
public:
    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Update();

	// キーの押下状態を取得する
    bool PushKey(uint8_t keyNumber);
	// キーの離上状態を取得する
    bool ReleaseKey(uint8_t keyNumber);
	// キーの押下トリガー状態を取得する
    bool TriggerKey(uint8_t keyNumber);
	// キーの離上トリガー状態を取得する
    bool ExitKey(uint8_t keyNumber);

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

    BYTE key_[256] = {};
    BYTE preKey_[256] = {};
};