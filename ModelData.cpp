#include "ModelData.h"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

/// <summary>
/// OBJファイルを読み込む
/// </summary>
/// <param name="directoryPath">OBJファイルがあるフォルダ</param>
/// <param name="filename">OBJファイル名</param>
/// <returns>読み込んだモデルデータ</returns>
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	// 読み込んだモデルデータを格納する
	ModelData modelData;

	// OBJファイル内の頂点座標を格納する
	std::vector<Vector4> positions;

	// OBJファイル内の法線を格納する
	std::vector<Vector3> normals;

	// OBJファイル内のテクスチャ座標を格納する
	std::vector<Vector2> texcoords;

	// ファイルから読み込んだ1行を格納する
	std::string line;

	// OBJファイルを開く
	std::ifstream file(directoryPath + "/" + filename);

	// ファイルが開けなかった場合は停止する
	assert(file.is_open());

	// ファイルを最後まで1行ずつ読み込む
	while (std::getline(file, line))
	{
		// 行の先頭にある識別子を格納する
		std::string identifier;

		// 読み込んだ1行を解析するためのストリーム
		std::istringstream s(line);

		// 行の先頭の識別子を読み込む
		s >> identifier;

		// ------------------------------
		// 頂点座標を読み込む
		// ------------------------------
		if (identifier == "v")
		{
			Vector4 position{};

			// X、Y、Z座標を読み込む
			s >> position.x
				>> position.y
				>> position.z;

			// OBJの右手座標系から
			// DirectXで使用する左手座標系へ変換する
			position.x *= -1.0f;

			// 同次座標のWを1にする
			position.w = 1.0f;

			// 頂点座標一覧へ追加する
			positions.push_back(position);
		}
		// ------------------------------
		// テクスチャ座標を読み込む
		// ------------------------------
		else if (identifier == "vt")
		{
			Vector2 texcoord{};

			// U、V座標を読み込む
			s >> texcoord.x
				>> texcoord.y;

			// OBJとDirectXではV方向が逆なので反転する
			texcoord.y = 1.0f - texcoord.y;

			// テクスチャ座標一覧へ追加する
			texcoords.push_back(texcoord);
		}
		// ------------------------------
		// 法線を読み込む
		// ------------------------------
		else if (identifier == "vn")
		{
			Vector3 normal{};

			// 法線のX、Y、Zを読み込む
			s >> normal.x
				>> normal.y
				>> normal.z;

			// OBJの右手座標系から
			// DirectXで使用する左手座標系へ変換する
			normal.x *= -1.0f;

			// 法線一覧へ追加する
			normals.push_back(normal);
		}
		// ------------------------------
		// マテリアルテンプレートファイルを読み込む
		// ------------------------------
		else if (identifier == "mtllib")
		{
			std::string materialFilename;

			s >> materialFilename;

			modelData.material =
				LoadMaterialTemplateFile(
					directoryPath,
					materialFilename);
		}
		// ------------------------------
		// 面情報を読み込む
		// ------------------------------
		else if (identifier == "f")
		{
			// 三角形の3頂点を一時的に保存する
			VertexData triangle[3]{};

			// OBJの面は三角形限定として3頂点読み込む
			for (int32_t faceVertex = 0;
				faceVertex < 3;
				++faceVertex)
			{
				// 「位置Index/UVIndex/法線Index」を格納する
				std::string vertexDefinition;

				// 1頂点分の定義を読み込む
				s >> vertexDefinition;

				// スラッシュ区切りで解析するためのストリーム
				std::istringstream v(vertexDefinition);

				// 0: 頂点座標
				// 1: テクスチャ座標
				// 2: 法線
				uint32_t elementIndices[3]{};

				// 位置、UV、法線の3つのIndexを読み込む
				for (int32_t element = 0;
					element < 3;
					++element)
				{
					std::string index;

					// 「/」を区切り文字としてIndexを読み込む
					std::getline(
						v,
						index,
						'/');

					// 文字列を数値へ変換する
					elementIndices[element] =
						static_cast<uint32_t>(
							std::stoi(index));
				}

				// OBJのIndexは1から始まるため、
				// 配列アクセス時には1を引く
				Vector4 position =
					positions[
						elementIndices[0] - 1];

				Vector2 texcoord =
					texcoords[
						elementIndices[1] - 1];

				Vector3 normal =
					normals[
						elementIndices[2] - 1];

				// 1頂点分のデータを作成する
				triangle[faceVertex] = {
					position,
					texcoord,
					normal
				};
			}

			// 右手系から左手系へ変換すると
			// 三角形の表裏が逆になるため、
			// 頂点を逆順に登録する
			modelData.vertices.push_back(
				triangle[2]);

			modelData.vertices.push_back(
				triangle[1]);

			modelData.vertices.push_back(
				triangle[0]);
		}
	}

	// 作成したモデルデータを返す
	return modelData;
}