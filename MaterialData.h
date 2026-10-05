#pragma once
#include <string>

/// <summary>
/// マテリアルデータ
/// </summary>
struct MaterialData
{
    std::string textureFilePath;
};

/// <summary>
/// マテリアルテンプレートファイルを読み込む
/// </summary>
/// <param name="directoryPath">ファイルがあるフォルダ</param>
/// <param name="filename">ファイル名</param>
/// <returns>読み込んだマテリアルデータ</returns>
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);