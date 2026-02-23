#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "encodings.hpp"
#include "representations.hpp"

namespace mrd {

struct Mesh {
	Connectivity connectivity = Connectivity::Triangle_UInt3_32b;
	R3 position = R3::Float3_32b;
	S2 normal = S2::Float3_32b;
	R2 uv = R2::Float2_32b;

	glm::mat4 transform { 1.0f };

	std::vector <std::byte> positions;
	std::vector <std::byte> normals;
	std::vector <std::byte> uvs;
	std::vector <std::byte> primitives;

	size_t position_stride() const { return encoding_size_bytes(position); }
	size_t normal_stride() const { return encoding_size_bytes(normal); }
	size_t uv_stride() const { return encoding_size_bytes(uv); }
	size_t primitive_stride() const { return encoding_size_bytes(connectivity); }

	size_t position_count() const;
	size_t normal_count() const;
	size_t uv_count() const;
	size_t primitive_count() const;

	size_t size_bytes() const;

	template <typename T>
	auto positions_as() -> std::span <T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(position_stride() == sizeof(T));
		assert((positions.size() % sizeof(T)) == 0);
		return std::span <T> (
			reinterpret_cast <T *> (positions.data()),
			positions.size() / sizeof(T)
		);
	}

	template <typename T>
	auto positions_as() const -> std::span <const T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(position_stride() == sizeof(T));
		assert((positions.size() % sizeof(T)) == 0);
		return std::span <const T> (
			reinterpret_cast <const T *> (positions.data()),
			positions.size() / sizeof(T)
		);
	}

	template <typename T>
	auto normals_as() -> std::span <T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(normal_stride() == sizeof(T));
		assert((normals.size() % sizeof(T)) == 0);
		return std::span <T> (
			reinterpret_cast <T *> (normals.data()),
			normals.size() / sizeof(T)
		);
	}

	template <typename T>
	auto normals_as() const -> std::span <const T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(normal_stride() == sizeof(T));
		assert((normals.size() % sizeof(T)) == 0);
		return std::span <const T> (
			reinterpret_cast <const T *> (normals.data()),
			normals.size() / sizeof(T)
		);
	}

	template <typename T>
	auto uvs_as() -> std::span <T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(uv_stride() == sizeof(T));
		assert((uvs.size() % sizeof(T)) == 0);
		return std::span <T> (
			reinterpret_cast <T *> (uvs.data()),
			uvs.size() / sizeof(T)
		);
	}

	template <typename T>
	auto uvs_as() const -> std::span <const T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(uv_stride() == sizeof(T));
		assert((uvs.size() % sizeof(T)) == 0);
		return std::span <const T> (
			reinterpret_cast <const T *> (uvs.data()),
			uvs.size() / sizeof(T)
		);
	}

	template <typename T>
	auto primitives_as() -> std::span <T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(primitive_stride() == sizeof(T));
		assert((primitives.size() % sizeof(T)) == 0);
		return std::span <T> (
			reinterpret_cast <T *> (primitives.data()),
			primitives.size() / sizeof(T)
		);
	}

	template <typename T>
	auto primitives_as() const -> std::span <const T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(primitive_stride() == sizeof(T));
		assert((primitives.size() % sizeof(T)) == 0);
		return std::span <const T> (
			reinterpret_cast <const T *> (primitives.data()),
			primitives.size() / sizeof(T)
		);
	}

	void deduplicate();
	void recalculate_normals();

	static auto box(glm::vec3 extent = glm::vec3(1.0f)) -> Mesh;
	static auto uv_sphere(float radius = 1.0f, int rings = 24, int segments = 48) -> Mesh;
	static auto ico_sphere(float radius = 1.0f, int subdivisions = 2) -> Mesh;
	static auto cylinder(float radius = 1.0f, float height = 1.0f, int slices = 32, int stacks = 1, bool caps = true) -> Mesh;
};

auto merge_meshes(const std::span <const Mesh> meshes) -> Mesh;

} // namespace mrd
