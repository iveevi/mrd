#include <cmath>
#include <limits>

#include <fmt/printf.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "model.hpp"

namespace mrd {

struct float3_64_raw {
	double x;
	double y;
	double z;
};

size_t Model::size_bytes() const
{
	size_t result = 0;
	for (const auto &mesh : meshes)
		result += mesh.size_bytes();
	return result;
}

auto Model::bounds() const -> std::pair <glm::vec3, glm::vec3>
{
	auto minv = glm::vec3(std::numeric_limits <float> ::max());
	auto maxv = glm::vec3(std::numeric_limits <float> ::lowest());

	for (const auto &mesh : meshes) {
		if (mesh.position == R3::Float3_32b) {
			for (const auto &p : mesh.positions_as <const float3_32b> ()) {
				auto v = glm::vec3(p.x, p.y, p.z);
				minv = glm::min(minv, v);
				maxv = glm::max(maxv, v);
			}
		} else if (mesh.position == R3::Float3_64b) {
			for (const auto &p : mesh.positions_as <const float3_64_raw> ()) {
				auto v = glm::vec3(float(p.x), float(p.y), float(p.z));
				minv = glm::min(minv, v);
				maxv = glm::max(maxv, v);
			}
		}
	}

	if (minv.x == std::numeric_limits <float> ::max()) {
		minv = glm::vec3(-1.0f);
		maxv = glm::vec3(1.0f);
	}

	return std::pair(minv, maxv);
}

auto Model::load(
	const std::filesystem::path &path,
	Connectivity model_connectivity,
	R3 model_position,
	S2 model_normal,
	R2 model_uv
) -> Model
{
	auto result = Model {};
	result.connectivity = model_connectivity;
	result.position = model_position;
	result.normal = model_normal;
	result.uv = model_uv;
	result.directory = path.parent_path();

	auto to_glm = [](const aiMatrix4x4 &m) -> glm::mat4 {
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	};

	auto append_position_f32 = [](Mesh &mesh, const float3_32b value) {
		auto idx = mesh.position_count();
		mesh.positions.resize((idx + 1) * sizeof(float3_32b));
		mesh.positions_as <float3_32b> ()[idx] = value;
	};

	auto append_position_f64 = [](Mesh &mesh, const float3_64_raw value) {
		auto idx = mesh.position_count();
		mesh.positions.resize((idx + 1) * sizeof(float3_64_raw));
		mesh.positions_as <float3_64_raw> ()[idx] = value;
	};

	auto append_normal_f32 = [](Mesh &mesh, const float3_32b value) {
		auto idx = mesh.normal_count();
		mesh.normals.resize((idx + 1) * sizeof(float3_32b));
		mesh.normals_as <float3_32b> ()[idx] = value;
	};

	auto append_normal_spherical = [](Mesh &mesh, const float2_32b value) {
		auto idx = mesh.normal_count();
		mesh.normals.resize((idx + 1) * sizeof(float2_32b));
		mesh.normals_as <float2_32b> ()[idx] = value;
	};

	auto append_uv_f32 = [](Mesh &mesh, const float2_32b value) {
		auto idx = mesh.uv_count();
		mesh.uvs.resize((idx + 1) * sizeof(float2_32b));
		mesh.uvs_as <float2_32b> ()[idx] = value;
	};

	auto append_primitive_u32 = [](Mesh &mesh, const uint3_32b value) {
		auto idx = mesh.primitive_count();
		mesh.primitives.resize((idx + 1) * sizeof(uint3_32b));
		mesh.primitives_as <uint3_32b> ()[idx] = value;
	};

	auto append_primitive_i32 = [](Mesh &mesh, const int3_32b value) {
		auto idx = mesh.primitive_count();
		mesh.primitives.resize((idx + 1) * sizeof(int3_32b));
		mesh.primitives_as <int3_32b> ()[idx] = value;
	};

	auto callbacks = impl::AssimpCallbacks {
		.new_mesh = [&](const aiMesh *mesh, const aiMatrix4x4 &xf) {
			auto m = Mesh {};
			m.connectivity = result.connectivity;
			m.position = result.position;
			m.normal = result.normal;
			m.uv = result.uv;
			m.transform = to_glm(xf);
			result.meshes.push_back(std::move(m));
			result.mesh_material_indices.push_back(mesh->mMaterialIndex);
		},
			.new_vertex = [&](const aiVector3D &v,
				  const std::optional <aiVector3D> &n,
				  const std::optional <aiVector3D> &t) {
			auto &mesh = result.meshes.back();

			if (mesh.position == R3::Float3_32b)
				append_position_f32(mesh, float3_32b(v.x, v.y, v.z));
			else if (mesh.position == R3::Float3_64b)
				append_position_f64(mesh, float3_64_raw(v.x, v.y, v.z));

			if (mesh.normal == S2::Float3_32b) {
				auto nv = n.value_or(aiVector3D(0.0f, 1.0f, 0.0f));
				append_normal_f32(mesh, float3_32b(nv.x, nv.y, nv.z));
			} else if (mesh.normal == S2::Spherical_32b) {
				auto nv = n.value_or(aiVector3D(0.0f, 1.0f, 0.0f));
				auto x = glm::vec3(nv.x, nv.y, nv.z);
				auto len = glm::length(x);
				if (len > 0.0f)
					x /= len;
				auto theta = std::atan2(x.z, x.x);
				auto phi = std::asin(glm::clamp(x.y, -1.0f, 1.0f));
				append_normal_spherical(mesh, float2_32b(theta, phi));
			}

			if (mesh.uv == R2::Float2_32b) {
				auto tv = t.value_or(aiVector3D(0.0f));
				append_uv_f32(mesh, float2_32b(tv.x, tv.y));
			}
		},
		.new_triangle = [&](const unsigned int tri[3]) {
			auto &mesh = result.meshes.back();
			if (mesh.connectivity == Connectivity::Triangle_UInt3_32b)
				append_primitive_u32(mesh, uint3_32b(tri[0], tri[1], tri[2]));
			else
				append_primitive_i32(mesh, int3_32b(int32_t(tri[0]), int32_t(tri[1]), int32_t(tri[2])));
		},
		.material = [&](uint32_t idx, const aiMaterial *mat, const std::filesystem::path &dir) {
			if (idx >= result.materials.size())
				result.materials.resize(idx + 1);

			auto &material = result.materials[idx];

			aiColor3D color;
			if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
				material.albedo.value = { color.r, color.g, color.b, 1.0f };
			if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS)
				material.specular.value = { color.r, color.g, color.b };

			float rough = 0.0f;
			if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, rough) == aiReturn_SUCCESS)
				material.roughness.value = rough;

			aiString tex_path;
			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path) == aiReturn_SUCCESS)
				material.albedo.path = dir / tex_path.C_Str();
			if (mat->GetTexture(aiTextureType_SPECULAR, 0, &tex_path) == aiReturn_SUCCESS)
				material.specular.path = dir / tex_path.C_Str();
			if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &tex_path) == aiReturn_SUCCESS)
				material.roughness.path = dir / tex_path.C_Str();
		},
	};

	impl::assimp_load(callbacks, path);
	return result;
}

} // namespace mrd

namespace mrd::impl {

void assimp_mesh(
	const AssimpCallbacks &callbacks,
	const aiMesh *const mesh,
	const aiScene *const scene,
	const std::filesystem::path &directory,
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
			continue;
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

	auto identity = aiMatrix4x4();
	return assimp_node(callbacks, scene->mRootNode, scene, directory, identity);
}

} // namespace mrd::impl
