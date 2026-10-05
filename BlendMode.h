#pragma once

enum BlendMode {
    // ブレンドなし
    kBlendModeNone,
    // 通常ブレンド
    kBlendModeNormal,
    // 加算ブレンド
    kBlendModeAdd,
    // 減算ブレンド
    kBlendModeSub,
    // 乗算ブレンド
    kBlendModeMul,
    // スクリーンブレンド
    kBlendModeScreen,
    // ブレンドモードの数
    kCountOfBlendMode
};