#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <functional>
#include <optional>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/material.h>
#include <assimp/mesh.h>

#include "mesh.hpp"

namespace mrd {

namespace impl {

struct AssimpCallbacks {
	std::function <void (const aiMesh *, const aiMatrix4x4 &)> new_mesh;
	std::function <void (
		const aiVector3D &,
		const std::optional <aiVector3D> &,
		const std::optional <aiVector3D> &
	)> new_vertex;
	std::function <void (const unsigned int [3])> new_triangle;
	std::function <void (uint32_t, const aiMaterial *, const std::filesystem::path &)> material;
};

void assimp_load(const AssimpCallbacks &callbacks, const std::filesystem::path &path);

} // namespace impl

template <typename T>
struct potential_texture {
	T value {};
	std::filesystem::path path;
	bool has_texture() const { return !path.empty(); }
};

struct Material {
	potential_texture <glm::vec4> albedo;
	potential_texture <glm::vec3> specular;
	potential_texture <float> roughness;

	Material() {
		albedo.value = glm::vec4(1.0f);
		specular.value = glm::vec3(1.0f);
		roughness.value = 0.5f;
	}
};

template <MeshEncodings F>
struct Model {
	using mesh_t = Mesh <F>;

	std::vector <mesh_t> meshes;
	std::vector <Material> materials;
	std::vector <uint32_t> mesh_material_indices;
	std::filesystem::path directory;

	size_t size_bytes() const {
		size_t result = 0;
		for (const auto &mesh : meshes)
			result += mesh.size_bytes();
		return result;
	}

	static Model load(const std::filesystem::path &path);
};

} // namespace mrd
