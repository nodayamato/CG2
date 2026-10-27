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
/// AssimpのNodeを解析する
/// </summary>
/// <param name="node">解析するNode</param>
/// <returns>解析されたNode</returns>
Node ReadNode(aiNode* node) {

	Node result{};

	// ------------------------------
	// LocalMatrixを取得
	// ------------------------------
	aiMatrix4x4 aiLocalMatrix = node->mTransformation;

	// Assimpは列ベクトル形式なので
	// DirectX用の行ベクトル形式へ転置
	aiLocalMatrix.Transpose();

	// ------------------------------
	// Matrix4x4へ変換
	// ------------------------------
	result.localMatrix.m[0][0] = aiLocalMatrix.a1;
	result.localMatrix.m[0][1] = aiLocalMatrix.a2;
	result.localMatrix.m[0][2] = aiLocalMatrix.a3;
	result.localMatrix.m[0][3] = aiLocalMatrix.a4;

	result.localMatrix.m[1][0] = aiLocalMatrix.b1;
	result.localMatrix.m[1][1] = aiLocalMatrix.b2;
	result.localMatrix.m[1][2] = aiLocalMatrix.b3;
	result.localMatrix.m[1][3] = aiLocalMatrix.b4;

	result.localMatrix.m[2][0] = aiLocalMatrix.c1;
	result.localMatrix.m[2][1] = aiLocalMatrix.c2;
	result.localMatrix.m[2][2] = aiLocalMatrix.c3;
	result.localMatrix.m[2][3] = aiLocalMatrix.c4;

	result.localMatrix.m[3][0] = aiLocalMatrix.d1;
	result.localMatrix.m[3][1] = aiLocalMatrix.d2;
	result.localMatrix.m[3][2] = aiLocalMatrix.d3;
	result.localMatrix.m[3][3] = aiLocalMatrix.d4;

	// ------------------------------
	// Node名
	// ------------------------------
	result.name = node->mName.C_Str();

	// ------------------------------
	// 子Nodeの数だけ確保
	// ------------------------------
	result.children.resize(
		node->mNumChildren
	);

	// ------------------------------
	// 子Nodeを再帰的に解析
	// ------------------------------
	for (
		uint32_t childIndex = 0;
		childIndex < node->mNumChildren;
		++childIndex
		) {

		result.children[childIndex] =
			ReadNode(
				node->mChildren[childIndex]
			);
	}

	return result;
}

/// <summary>
/// モデルファイルを読み込む
/// </summary>
/// <param name="directoryPath">モデルファイルがあるフォルダ</param>
/// <param name="filename">モデルファイル名</param>
/// <returns>読み込んだモデルデータ</returns>
ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename) {

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

	// ------------------------------
	// RootNodeを解析
	// ------------------------------
	modelData.rootNode =
		ReadNode(
			scene->mRootNode
		);

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