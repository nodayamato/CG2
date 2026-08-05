#pragma once
#include "MaterialData.h"
#include <vector>
#include <string>
#include "Math.h"

/// <summary>
/// モデルデータ
/// </summary>
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

/// <summary>
/// .objファイルを読み込む
/// </summary>
/// <param name="directoryPath">ディレクトリパス</param>
/// <param name="filename">ファイル名</param>
/// <returns>モデルデータ</returns>
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);