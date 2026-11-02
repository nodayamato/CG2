#include "Logger.h"
#include "ConvertString.h"

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <format>

// =====================================
// ログファイル
// =====================================
namespace {
    std::ofstream logStream;
}

// =====================================
// ログ初期化
// =====================================
void Logger::Initialize() {

    // logsフォルダを作成
    std::filesystem::create_directories("logs");

    // 現在時刻を取得
    auto now = std::chrono::system_clock::now();

    // 秒単位に変換
    auto nowSeconds =
        std::chrono::time_point_cast<std::chrono::seconds>(now);

    // ローカル時刻に変換
    std::chrono::zoned_time localTime{
        std::chrono::current_zone(),
        nowSeconds
    };

    // ログファイル名を作成
    std::string logFilePath =
        std::format(
            "logs/{:%Y%m%d_%H%M%S}.log",
            localTime
        );

    // ログファイルを開く
    logStream.open(logFilePath);
}

// =====================================
// ログ終了
// =====================================
void Logger::Finalize() {

    if (logStream.is_open()) {
        logStream.flush();
        logStream.close();
    }
}

// =====================================
// ログ出力
// =====================================
void Logger::Log(const std::string& message) {

    // ファイルに出力
    if (logStream.is_open()) {
        logStream << message << '\n';
        logStream.flush();
    }

    // Visual Studioの出力ウィンドウ
    OutputDebugStringA(
        (message + "\n").c_str()
    );
}

// =====================================
// ワイド文字列ログ
// =====================================
void Logger::Log(const std::wstring& message) {

    Log(ConvertString(message));
}