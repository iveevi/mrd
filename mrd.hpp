#pragma once

#include <vector>

#include <filesystem>
#include <optional>

#include <glm/glm.hpp>

// TODO: users responsbility to enable 64 bit support
// through cmake macro defines... or smth
#include <assimp/vector3.h>

namespace mrd {

namespace encodings {

enum class R3 {
	Float3_32b,
	Float3_64b,
};

enum class R2 {
	Disable,
	Float2_32b,
};

enum class S2 {
	Disable,
	Float3_32b,
	Spherical_32b,
};

enum class Connectivity {
	Triangle_Int3_32b,
	Triangle_UInt3_32b,
};

enum class Texture {
	SFloat_R32b_G32b_B32b_A32b,
	SInt_R32b_G32b_B32b_A32b,
	UInt_R32b_G32b_B32b_A32b,
};

// TODO: conversion from vulkan format to Texture format...

} // namespace encodings

template <typename T>
struct basic_vector_space_d2 : public T {
	basic_vector_space_d2() = default;
	basic_vector_space_d2(auto x, auto y) : T(x, y) {}
	basic_vector_space_d2(const T &other) : T(other) {}

	friend T operator+(const T &a, const T &b) {
		return T { a.x + b.x, a.y + b.y };
	}
};

template <typename T>
struct basic_vector_space_d3 : public T {
	basic_vector_space_d3() = default;
	basic_vector_space_d3(auto x, auto y, auto z) : T(x, y, z) {}
	basic_vector_space_d3(const T &other) : T(other) {}

	friend T operator+(const T &a, const T &b) {
		return T { a.x + b.x, a.y + b.y, a.z + b.z };
	}
};

// TODO: also real_vector_space_d3 for additional operations...

namespace representations {

struct int3_32b {
	glm::int32_t x;
	glm::int32_t y;
	glm::int32_t z;
};

struct uint3_32b {
	glm::uint32_t x;
	glm::uint32_t y;
	glm::uint32_t z;
};

struct float2_32b {
	glm::float32_t x;
	glm::float32_t y;
};

struct float3_32b {
	glm::float32_t x;
	glm::float32_t y;
	glm::float32_t z;
};

struct float4_32b {
	glm::float32_t x;
	glm::float32_t y;
	glm::float32_t z;
	glm::float32_t w;
};

} // namespace repr

using int3_32b = basic_vector_space_d3 <representations::int3_32b>;
using uint3_32b = basic_vector_space_d3 <representations::uint3_32b>;

using float2_32b = basic_vector_space_d2 <representations::float2_32b>;
using float3_32b = basic_vector_space_d3 <representations::float3_32b>;

// NOTE: we map from encoding to type instead of templating on the encoding
// since some encodings are implemented the same (e.g. R3::float3_32b and S2::float3_32b);
// we want to avoid going through the hassle of implementing conversion operators
// for such cases...

template <auto E>
struct encoding_representation {};

#define ENCODING_REPRESENTATION(E, T) template <> struct encoding_representation <encodings::E> { using type = T; };

ENCODING_REPRESENTATION(R2::Float2_32b, float2_32b);
ENCODING_REPRESENTATION(R3::Float3_32b, float3_32b);

ENCODING_REPRESENTATION(S2::Float3_32b, float3_32b);
ENCODING_REPRESENTATION(S2::Disable, std::nullptr_t);

ENCODING_REPRESENTATION(Connectivity::Triangle_Int3_32b, int3_32b);
ENCODING_REPRESENTATION(Connectivity::Triangle_UInt3_32b, uint3_32b);

template <auto E>
using encoding_representation_t = typename encoding_representation <E> ::type;

struct MeshEncodings {
	encodings::Connectivity connectivity = encodings::Connectivity::Triangle_UInt3_32b;
	encodings::R3 positions = encodings::R3::Float3_32b;
	encodings::S2 normals = encodings::S2::Float3_32b;
	encodings::R2 uvs = encodings::R2::Float2_32b;
};

struct MaterialEncodings {
	encodings::Texture albedo = encodings::Texture::SFloat_R32b_G32b_B32b_A32b;
	encodings::Texture specular = encodings::Texture::SFloat_R32b_G32b_B32b_A32b;
};

struct ModelEncodings {
	MeshEncodings meshes;
	// TODO: should be able to disable materials entirely...
	// TextureEncodings textures;
};

struct nil_normals_container_t {};
struct nil_uvs_container_t {};

// TODO: implementation as a streaming loader, that is then dynamically
// compressed/converted?

template <MeshEncodings F>
struct Mesh {
	using primitive_t = encoding_representation_t <F.connectivity>;
	using position_t = encoding_representation_t <F.positions>;
	using normal_t = encoding_representation_t <F.normals>;
	using uv_t = encoding_representation_t <F.uvs>;

	std::vector <position_t> positions;

	[[no_unique_address]] std::conditional_t <
		F.normals == encodings::S2::Disable,
		nil_normals_container_t,
		std::vector <normal_t>
	> normals;
	
	[[no_unique_address]] std::conditional_t <
		F.uvs == encodings::R2::Disable,
		nil_uvs_container_t,
		std::vector <uv_t>
	> uvs;

	std::vector <primitive_t> primitives;

	// TODO: methods: deduplication, conversion, etc.
};

// TODO: meshlet compaction...
// TODO: triangle fan encoding?

namespace impl {

struct AssimpCallbacks {
	std::function <void (
		// No arguments
	)> new_mesh;

	std::function <void (
		const aiVector3D &,
		const std::optional <aiVector3D> &,
		const std::optional <aiVector3D> &
	)> new_vertex;
	
	std::function <void (
		const unsigned int [3]
	)> new_triangle;
};

void assimp_load(const AssimpCallbacks &callbacks, const std::filesystem::path &path);

} // namespace impl

template <ModelEncodings F>
struct Model {
	using mesh_t = Mesh <F.meshes>;

	std::vector <mesh_t> meshes;
	// TODO: all the materials as well

	// TODO: for loading, we require a triangle primitive
	static Model load(const std::filesystem::path &path) {
		Model result;

		// TODO: reserve # of meshes with a quick scan
		// TODO: for each mesh, reserve # of vertices with a quick scan
		auto callbacks = impl::AssimpCallbacks {
			.new_mesh = [&]() {
				result.meshes.push_back(mesh_t());
			},
			.new_vertex = [&](const aiVector3D &v,
		     			  const std::optional <aiVector3D> &n,
					  const std::optional <aiVector3D> &t) {
				auto &mesh = result.meshes.back();
				
				mesh.positions.emplace_back(v.x, v.y, v.z);

				if constexpr (F.meshes.normals != encodings::S2::Disable) {
					auto nv = n.value_or(aiVector3D(0));
					mesh.normals.emplace_back(nv.x, nv.y, nv.z);
				}

				if constexpr (F.meshes.uvs != encodings::R2::Disable) {
					auto tv = t.value_or(aiVector3D(0));
					mesh.uvs.emplace_back(tv.x, tv.y);
				}
			},
			.new_triangle = [&](const unsigned int tri[3]) {
				auto &mesh = result.meshes.back();
				// TODO: this requires a triangle primitive...
				mesh.primitives.emplace_back(tri[0], tri[1], tri[2]);
			},
		};

		impl::assimp_load(callbacks, path);

		return result;
	}
};

template <encodings::Texture F>
struct Texture {
	// TODO: serde for this...
};

} // namespace mrd
