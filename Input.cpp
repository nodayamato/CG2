#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
    HRESULT hr;

    // DirectInputの初期化
    hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()),
        nullptr);

    assert(SUCCEEDED(hr));

    // キーボードデバイス生成
    hr = directInput_->CreateDevice(
        GUID_SysKeyboard,
        keyboard_.GetAddressOf(),
        nullptr);

    assert(SUCCEEDED(hr));

    // 入力形式
    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);

    assert(SUCCEEDED(hr));

    // 排他制御
    hr = keyboard_->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND |
        DISCL_NONEXCLUSIVE |
        DISCL_NOWINKEY);

    assert(SUCCEEDED(hr));
}

void Input::Update()
{
    memcpy(preKey_, key_, sizeof(key_));

    keyboard_->Acquire();

    keyboard_->GetDeviceState(
        sizeof(key_),
        key_);
}

bool Input::PushKey(uint8_t keyNumber)
{
    return key_[keyNumber];
}

bool Input::ReleaseKey(uint8_t keyNumber)
{
    return !key_[keyNumber];
}

bool Input::TriggerKey(uint8_t keyNumber)
{
    return key_[keyNumber] && !preKey_[keyNumber];
}

bool Input::ExitKey(uint8_t keyNumber)
{
    return !key_[keyNumber] && preKey_[keyNumber];
}