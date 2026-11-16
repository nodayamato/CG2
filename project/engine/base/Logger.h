#pragma once

#include <string>

// =====================================
// ログ管理クラス
// =====================================
class Logger {
public:
    // ログファイルの初期化
    static void Initialize();

    // ログファイルの終了
    static void Finalize();

    // 文字列ログ
    static void Log(const std::string& message);

    // ワイド文字列ログ
    static void Log(const std::wstring& message);
};