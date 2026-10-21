#include "ModelData.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

/// <summary>
/// OBJファイルを読み込む
/// </summary>
/// <param name="directoryPath">OBJファイルがあるフォルダ</param>
/// <param name="filename">OBJファイル名</param>
/// <returns>読み込んだモデルデータ</returns>
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {

	// ------------------------------
	// 読み込んだモデルデータ
	// ------------------------------
	ModelData modelData;

	// ------------------------------
	// Assimpを使ってファイルを読み込む
	// ------------------------------
	Assimp::Importer importer;

	std::string filePath =
		directoryPath + "/" + filename;

	const aiScene* scene =
		importer.ReadFile(
			filePath.c_str(),

			// 三角形の頂点順を反転する
			aiProcess_FlipWindingOrder |

			// UVのV方向を反転する
			aiProcess_FlipUVs |

			// 四角形などを三角形に変換する
			aiProcess_Triangulate
		);

	// ------------------------------
	// 読み込み確認
	// ------------------------------
	assert(scene != nullptr);

	// Meshが存在することを確認
	assert(scene->HasMeshes());

	// =====================================
	// Meshを解析する
	// =====================================

	for (
		uint32_t meshIndex = 0;
		meshIndex < scene->mNumMeshes;
		++meshIndex
		) {

		// ------------------------------
		// Meshを取得
		// ------------------------------
		aiMesh* mesh = scene->mMeshes[meshIndex];

		// 法線が存在することを確認
		assert(mesh->HasNormals());

		// UVが存在することを確認
		assert(mesh->HasTextureCoords(0));

		// =================================
		// Faceを解析する
		// =================================

		for (
			uint32_t faceIndex = 0;
			faceIndex < mesh->mNumFaces;
			++faceIndex
			) {

			// ------------------------------
			// Faceを取得
			// ------------------------------
			aiFace& face = mesh->mFaces[faceIndex];

			// Triangulateを使っているので3頂点
			assert(face.mNumIndices == 3);

			// =================================
			// Vertexを解析する
			// =================================

			for (
				uint32_t element = 0;
				element < face.mNumIndices;
				++element
				) {

				// ------------------------------
				// Faceから頂点Indexを取得
				// ------------------------------
				uint32_t vertexIndex = face.mIndices[element];

				// ------------------------------
				// Assimpから頂点情報を取得
				// ------------------------------

				aiVector3D& position = mesh->mVertices[vertexIndex];

				aiVector3D& normal = mesh->mNormals[vertexIndex];

				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				// ------------------------------
				// VertexDataを作る
				// ------------------------------
				VertexData vertex{};

				// 頂点座標
				vertex.position = {
					position.x,
					position.y,
					position.z,
					1.0f
				};

				// 法線
				vertex.normal = {
					normal.x,
					normal.y,
					normal.z
				};

				// UV
				vertex.texcoord = {
					texcoord.x,
					texcoord.y
				};

				// ------------------------------
				// 右手座標系 → 左手座標系
				// ------------------------------

				vertex.position.x *= -1.0f;

				vertex.normal.x *= -1.0f;

				// ------------------------------
				// ModelDataへ追加
				// ------------------------------
				modelData.vertices.push_back(
					vertex
				);
			}
		}
	}

	// =====================================
	// Materialを解析する
	// =====================================

	for (
		uint32_t materialIndex = 0;
		materialIndex < scene->mNumMaterials;
		++materialIndex
		) {

		// ------------------------------
		// Material取得
		// ------------------------------
		aiMaterial* material = scene->mMaterials[materialIndex];

		// ------------------------------
		// DiffuseTextureがあるか確認
		// ------------------------------
		if (
			material->GetTextureCount(
				aiTextureType_DIFFUSE
			) != 0
			) {

			// Textureパス
			aiString textureFilePath;

			// 0番目のDiffuseTextureを取得
			material->GetTexture(
				aiTextureType_DIFFUSE,
				0,
				&textureFilePath
			);

			// ------------------------------
			// Textureパスを保存
			// ------------------------------
			modelData.material.textureFilePath =
				directoryPath +
				"/" +
				textureFilePath.C_Str();
		}
	}

	// ------------------------------
	// 頂点が読み込めているか確認
	// ------------------------------
	assert(!modelData.vertices.empty());

	// ------------------------------
	// 完成したModelDataを返す
	// ------------------------------
	return modelData;
}