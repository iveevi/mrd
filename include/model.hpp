#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/vector3.h>

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

	Material()
	{
		albedo.value = glm::vec4(1.0f);
		specular.value = glm::vec3(1.0f);
		roughness.value = 0.5f;
	}
};

struct Model {
	Connectivity connectivity = Connectivity::Triangle_UInt3_32b;
	R3 position = R3::Float3_32b;
	S2 normal = S2::Float3_32b;
	R2 uv = R2::Float2_32b;

	std::vector <Mesh> meshes;
	std::vector <Material> materials;
	std::vector <uint32_t> mesh_material_indices;
	std::filesystem::path directory;

	size_t size_bytes() const;
	auto bounds() const -> std::pair <glm::vec3, glm::vec3>;

	static auto load(
		const std::filesystem::path &path,
		Connectivity connectivity = Connectivity::Triangle_UInt3_32b,
		R3 position = R3::Float3_32b,
		S2 normal = S2::Float3_32b,
		R2 uv = R2::Float2_32b
	) -> Model;
};

} // namespace mrd
