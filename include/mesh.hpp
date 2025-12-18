#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/constants.hpp>

#include "encodings.hpp"
#include "representations.hpp"

namespace mrd {

struct nil_normals_container_t {};
struct nil_uvs_container_t {};

template <MeshEncodings F>
struct Mesh {
	using primitive_t = encoding_representation_t <F.connectivity>;
	using position_t = encoding_representation_t <F.positions>;
	using normal_t = encoding_representation_t <F.normals>;
	using uv_t = encoding_representation_t <F.uvs>;
	using index_t = decltype(primitive_t{}.x);

	using position_list_t = std::vector <position_t>;
	using normal_list_t = std::conditional_t <
		F.normals == S2::Disable,
		nil_normals_container_t,
		std::vector <normal_t>
	>;
	using uv_list_t = std::conditional_t <
		F.uvs == R2::Disable,
		nil_uvs_container_t,
		std::vector <uv_t>
	>;
	using primitive_list_t = std::vector <primitive_t>;

	glm::mat4 transform { 1.0f };

	position_list_t positions;
	[[no_unique_address]] normal_list_t normals;
	[[no_unique_address]] uv_list_t uvs;
	primitive_list_t primitives;

	size_t size_bytes() const {
		size_t result = sizeof(transform);
		result += std::span(positions).size_bytes();
		if constexpr (F.normals != S2::Disable)
			result += std::span(normals).size_bytes();
		if constexpr (F.uvs != R2::Disable)
			result += std::span(uvs).size_bytes();

		result += std::span(primitives).size_bytes();
		return result;
	}

	void deduplicate();
	void recalculate_normals()
	requires (F.normals != S2::Disable);

	static Mesh box(glm::vec3 extent = glm::vec3(1.0f));
	static Mesh uv_sphere(float radius = 1.0f, int rings = 24, int segments = 48);
	static Mesh ico_sphere(float radius = 1.0f, int subdivisions = 2);
	static Mesh cylinder(float radius = 1.0f, float height = 1.0f, int slices = 32, int stacks = 1, bool caps = true);
};

} // namespace mrd
