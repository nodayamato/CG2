#pragma once
#include "MaterialData.h"
#include "Math.h"

#include <vector>
#include <string>

/// <summary>
/// Node情報
/// </summary>
struct Node {
	// Nodeのローカル行列
	Matrix4x4 localMatrix;
	// Nodeの名前
	std::string name;
	// 子Node
	std::vector<Node> children;
};

/// <summary>
/// モデルデータ
/// </summary>
struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
	Node rootNode;
};

/// <summary>
/// モデルファイルを読み込む
/// </summary>
/// <param name="directoryPath">ディレクトリパス</param>
/// <param name="filename">ファイル名</param>
/// <returns>モデルデータ</returns>
ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);