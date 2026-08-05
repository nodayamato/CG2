#include "MaterialData.h"
#include <cassert>
#include <fstream>
#include <sstream>

/// <summary>
/// マテリアルテンプレートファイルを読み込む
/// </summary>
/// <param name="directoryPath">ファイルがあるフォルダ</param>
/// <param name="filename">ファイル名</param>
/// <returns>読み込んだマテリアルデータ</returns>
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    MaterialData materialData;

    std::string line;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line))
    {
        std::string identifier;
        std::istringstream s(line);

        s >> identifier;

        if (identifier == "map_Kd")
        {
            std::string textureFilename;
            s >> textureFilename;

            materialData.textureFilePath =
                directoryPath + "/" + textureFilename;
        }
    }

    return materialData;
}