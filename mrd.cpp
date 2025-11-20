#include <fmt/printf.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mrd.hpp"

namespace mrd::impl {

void assimp_mesh(const AssimpCallbacks &callbacks,
		 const aiMesh *const mesh,
		 const aiScene *const scene,
		 const std::filesystem::path &direcory)
{
	callbacks.new_mesh();

	for (size_t i = 0; i < mesh->mNumVertices; i++) {
		aiVector3D position = mesh->mVertices[i];

		std::optional <aiVector3D> normal;
		if (mesh->HasNormals())
			normal = mesh->mNormals[i];

		std::optional <aiVector3D> uv;
		if (mesh->HasTextureCoords(0))
			uv = mesh->mTextureCoords[0][i];

		callbacks.new_vertex(position, normal, uv);
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++) {
		auto face = mesh->mFaces[i];
		assert(face.mNumIndices == 3);
		callbacks.new_triangle(face.mIndices);
	}
}

void assimp_node(const AssimpCallbacks &callbacks,
		 const aiNode *const node,
		 const aiScene *const scene,
		 const std::filesystem::path &directory)
{
	fmt::println("# of meshes in this node: {}", node->mNumMeshes);

	for (size_t i = 0; i < node->mNumMeshes; i++) {
		auto idx = node->mMeshes[i];
		assimp_mesh(callbacks, scene->mMeshes[idx], scene, directory);
	}

	for (size_t i = 0; i < node->mNumChildren; i++)
		assimp_node(callbacks, node->mChildren[i], scene, directory);
}

void assimp_load(const AssimpCallbacks &callbacks, const std::filesystem::path &path)
{
	Assimp::Importer importer;

	auto scene = importer.ReadFile(
		path,
		aiProcess_Triangulate
		| aiProcess_GenNormals
		| aiProcess_FlipUVs
	);

	if (!scene
		|| !scene->mRootNode
		|| scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
		// TODO: error callback
		fmt::println("assimp error: {}", importer.GetErrorString());
		return;
	}

	return assimp_node(callbacks, scene->mRootNode, scene, path.parent_path());
}

} // namespace mrd::impl
