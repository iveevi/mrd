#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <limits>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/material.h>
#include <assimp/mesh.h>

#include "model.hpp"

namespace mrd {

template <Connectivity Primitive, R3 Position, S2 Normal, R2 UV>
inline auto Model <Primitive, Position, Normal, UV> ::load(const std::filesystem::path &path)
{
	Model result;

	result.directory = path.parent_path();

	auto to_glm = [](const aiMatrix4x4 &m) -> glm::mat4 {
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	};

	auto callbacks = impl::AssimpCallbacks {
		.new_mesh = [&](const aiMesh *mesh, const aiMatrix4x4 &xf) {
			mesh_t m;
			m.transform = to_glm(xf);
			result.meshes.push_back(std::move(m));
			result.mesh_material_indices.push_back(mesh->mMaterialIndex);
		},
		.new_vertex = [&](const aiVector3D &v,
				  const std::optional <aiVector3D> &n,
				  const std::optional <aiVector3D> &t) {
			auto &mesh = result.meshes.back();

			mesh.positions.emplace_back(v.x, v.y, v.z);

			if constexpr (Normal != S2::Disable) {
				auto nv = n.value_or(aiVector3D(0));
				mesh.normals.emplace_back(nv.x, nv.y, nv.z);
			}

			if constexpr (UV != R2::Disable) {
				auto tv = t.value_or(aiVector3D(0));
				mesh.uvs.emplace_back(tv.x, tv.y);
			}
		},
		.new_triangle = [&](const unsigned int tri[3]) {
			auto &mesh = result.meshes.back();
			mesh.primitives.emplace_back(tri[0], tri[1], tri[2]);
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

			aiString texPath;
			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS) {
				material.albedo.path = dir / texPath.C_Str();
			}
			if (mat->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == aiReturn_SUCCESS) {
				material.specular.path = dir / texPath.C_Str();
			}
			// Assimp doesn't standardize roughness; try the ORM slot if present.
			if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &texPath) == aiReturn_SUCCESS) {
				material.roughness.path = dir / texPath.C_Str();
			}
		},
	};

	impl::assimp_load(callbacks, path);

	return result;
}

template <Connectivity Primitive, R3 Position, S2 Normal, R2 UV>
inline auto Model <Primitive, Position, Normal, UV> ::bounds() const
{
	auto minv = glm::vec3(std::numeric_limits <float> ::max());
	auto maxv = glm::vec3(std::numeric_limits <float> ::lowest());

	for (const auto &mesh : meshes) {
		for (const auto &pos : mesh.positions) {
			auto p = static_cast <glm::vec3> (pos);
			minv = glm::min(minv, p);
			maxv = glm::max(maxv, p);
		}
	}

	if (minv.x == std::numeric_limits <float> ::max()) {
		minv = glm::vec3(-1.0f);
		maxv = glm::vec3(1.0f);
	}

	return std::pair(minv, maxv);
}

} // namespace mrd
