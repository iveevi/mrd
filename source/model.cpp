#include <fmt/printf.h>
#include <cassert>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "model.inl"

namespace mrd::impl {

void assimp_mesh(
	const AssimpCallbacks &callbacks,
	const aiMesh *const mesh,
	const aiScene *const scene,
	const std::filesystem::path &direcory,
	const aiMatrix4x4 &xform
)
{
	callbacks.new_mesh(mesh, xform);

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
		if (face.mNumIndices != 3)
			continue; // skip non-triangle faces (assimp should triangulate)
		callbacks.new_triangle(face.mIndices);
	}
}

void assimp_node(
	const AssimpCallbacks &callbacks,
	const aiNode *const node,
	const aiScene *const scene,
	const std::filesystem::path &directory,
	const aiMatrix4x4 &parent_xform
)
{
	auto node_xform = parent_xform * node->mTransformation;

	for (size_t i = 0; i < node->mNumMeshes; i++) {
		auto idx = node->mMeshes[i];
		assimp_mesh(callbacks, scene->mMeshes[idx], scene, directory, node_xform);
	}

	for (size_t i = 0; i < node->mNumChildren; i++)
		assimp_node(callbacks, node->mChildren[i], scene, directory, node_xform);
}

void assimp_load(const AssimpCallbacks &callbacks, const std::filesystem::path &path)
{
	Assimp::Importer importer;

	auto scene = importer.ReadFile(
		path,
		aiProcess_Triangulate
		| aiProcess_FlipUVs
	);

	if (!scene
		|| !scene->mRootNode
		|| scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
		fmt::println("assimp error: {}", importer.GetErrorString());
		return;
	}

	auto directory = path.parent_path();
	if (callbacks.material) {
		for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
			callbacks.material(i, scene->mMaterials[i], directory);
	}

	aiMatrix4x4 identity;
	identity = aiMatrix4x4(); // identity by default

	return assimp_node(callbacks, scene->mRootNode, scene, directory, identity);
}

} // namespace mrd::impl
