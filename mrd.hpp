#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <filesystem>
#include <optional>

// TODO: eventually we should get rid of glm dependencies
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

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

	constexpr bool operator==(const float3_32b &rhs) const noexcept {
		return std::bit_cast <uint32_t> (x) == std::bit_cast <uint32_t> (rhs.x)
		    && std::bit_cast <uint32_t> (y) == std::bit_cast <uint32_t> (rhs.y)
		    && std::bit_cast <uint32_t> (z) == std::bit_cast <uint32_t> (rhs.z);
	}

	static size_t hash(const float3_32b &p) noexcept {
		const auto bx = std::bit_cast <uint32_t> (p.x);
		const auto by = std::bit_cast <uint32_t> (p.y);
		const auto bz = std::bit_cast <uint32_t> (p.z);
		return (static_cast <size_t> (bx) * 0x1f1f1f1fu)
		     ^ (static_cast <size_t> (by) * 0x3b3b3b3bu)
		     ^ (static_cast <size_t> (bz) * 0x5f5f5f5fu);
	}

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
	using index_t = decltype(primitive_t{}.x);

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

	size_t size_bytes() const {
		size_t result = std::span(positions).size_bytes();
		if constexpr (F.normals != encodings::S2::Disable)
			result += std::span(normals).size_bytes();
		if constexpr (F.uvs != encodings::R2::Disable)
			result += std::span(uvs).size_bytes();

		result += std::span(primitives).size_bytes();
		return result;
	}

	// TODO: later add flag parameters for more than positions?
	void deduplicate() {
		if (positions.empty())
			return;

		auto hash = [](const position_t &p) noexcept {
			return position_t::hash(p);
		};

		auto eq = [](const position_t &a, const position_t &b) noexcept {
			return a == b;
		};

		std::unordered_map <position_t, size_t, decltype(hash), decltype(eq)> remap(
			positions.size(), hash, eq
		);

		constexpr bool has_normals = F.normals != encodings::S2::Disable;
		constexpr bool has_uvs = F.uvs != encodings::R2::Disable;

		std::vector <position_t> new_positions;
		new_positions.reserve(positions.size());

		decltype(normals) new_normals;
		if constexpr (has_normals)
			new_normals.reserve(normals.size());

		decltype(uvs) new_uvs;
		if constexpr (has_uvs)
			new_uvs.reserve(uvs.size());

		auto get_or_insert = [&](size_t idx) -> size_t {
			const auto &p = positions[idx];
			auto [it, inserted] = remap.try_emplace(p, new_positions.size());
			if (inserted) {
				new_positions.push_back(p);
				if constexpr (has_normals)
					new_normals.push_back(normals[idx]);
				if constexpr (has_uvs)
					new_uvs.push_back(uvs[idx]);
			}
			return it->second;
		};

		for (auto &prim : primitives) {
			prim.x = static_cast <decltype(prim.x)> (get_or_insert(prim.x));
			prim.y = static_cast <decltype(prim.y)> (get_or_insert(prim.y));
			prim.z = static_cast <decltype(prim.z)> (get_or_insert(prim.z));
		}

		positions = std::move(new_positions);
		if constexpr (has_normals)
			normals = std::move(new_normals);
		if constexpr (has_uvs)
			uvs = std::move(new_uvs);
	}

	void recalculate_normals()
	requires (F.normals != encodings::S2::Disable) {
		normals.assign(positions.size(), normal_t(0.0f, 0.0f, 0.0f));

		if (primitives.empty())
			return;

		// TODO: add methods to the accum stuff...
		std::vector <glm::vec3> accum(positions.size(), glm::vec3(0.0f));

		for (const auto &tri : primitives) {
			auto i0 = static_cast <size_t> (tri.x);
			auto i1 = static_cast <size_t> (tri.y);
			auto i2 = static_cast <size_t> (tri.z);

			if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
				continue; // skip invalid triangles

			glm::vec3 p0 { positions[i0].x, positions[i0].y, positions[i0].z };
			glm::vec3 p1 { positions[i1].x, positions[i1].y, positions[i1].z };
			glm::vec3 p2 { positions[i2].x, positions[i2].y, positions[i2].z };

			glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
			if (glm::dot(n, n) == 0.0f)
				continue;
			n = glm::normalize(n);

			accum[i0] += n;
			accum[i1] += n;
			accum[i2] += n;
		}

		for (size_t i = 0; i < accum.size(); ++i) {
			glm::vec3 n = accum[i];
			if (glm::dot(n, n) == 0.0f)
				n = glm::vec3(0.0f, 1.0f, 0.0f);
			else
				n = glm::normalize(n);

			normals[i] = normal_t(n.x, n.y, n.z);
		}
	}

	static Mesh box(glm::vec3 extent = glm::vec3(1.0f)) {
		Mesh mesh;

		const glm::vec3 he = 0.5f * extent;

		auto push = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uv) {
			mesh.positions.emplace_back(p.x, p.y, p.z);
			if constexpr (F.normals != encodings::S2::Disable)
				mesh.normals.emplace_back(n.x, n.y, n.z);
			if constexpr (F.uvs != encodings::R2::Disable)
				mesh.uvs.emplace_back(uv.x, uv.y);
		};

		auto add_face = [&](glm::vec3 n, glm::vec3 u, glm::vec3 v) {
			glm::vec3 center = glm::vec3(
				n.x * he.x,
				n.y * he.y,
				n.z * he.z
			);

			glm::vec3 du = glm::vec3(u.x * he.x, u.y * he.y, u.z * he.z);
			glm::vec3 dv = glm::vec3(v.x * he.x, v.y * he.y, v.z * he.z);

			glm::vec3 v0 = center + du + dv;
			glm::vec3 v1 = center - du + dv;
			glm::vec3 v2 = center - du - dv;
			glm::vec3 v3 = center + du - dv;

			size_t base = mesh.positions.size();

			push(v0, n, { 0, 0 });
			push(v1, n, { 1, 0 });
			push(v2, n, { 1, 1 });
			push(v3, n, { 0, 1 });

			mesh.primitives.emplace_back(
				static_cast <index_t> (base + 0),
				static_cast <index_t> (base + 1),
				static_cast <index_t> (base + 2)
			);
			mesh.primitives.emplace_back(
				static_cast <index_t> (base + 0),
				static_cast <index_t> (base + 2),
				static_cast <index_t> (base + 3)
			);
		};

		add_face({ 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }); // +X
		add_face({ -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }); // -X
		add_face({ 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 }); // +Y
		add_face({ 0, -1, 0 }, { 0, 0, -1 }, { 1, 0, 0 }); // -Y
		add_face({ 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }); // +Z
		add_face({ 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 }); // -Z

		return mesh;
	}

	static Mesh uv_sphere(float radius = 1.0f, int rings = 24, int segments = 48) {
		Mesh mesh;
		rings = std::max(3, rings);
		segments = std::max(3, segments);

		for (int r = 0; r <= rings; ++r) {
			float v = float(r) / rings;
			float phi = v * glm::pi <float> ();
			float sinp = std::sin(phi);
			float cosp = std::cos(phi);

			for (int s = 0; s <= segments; ++s) {
				float u = float(s) / segments;
				float theta = u * (2.0f * glm::pi <float> ());
				float sint = std::sin(theta);
				float cost = std::cos(theta);

				glm::vec3 n = { cost * sinp, cosp, sint * sinp };
				glm::vec3 p = radius * n;

				mesh.positions.emplace_back(p.x, p.y, p.z);
				if constexpr (F.normals != encodings::S2::Disable)
					mesh.normals.emplace_back(n.x, n.y, n.z);
				if constexpr (F.uvs != encodings::R2::Disable)
					mesh.uvs.emplace_back(u, 1.0f - v);
			}
		}

		auto idx = [=](int r, int s) {
			return r * (segments + 1) + s;
		};

		for (int r = 0; r < rings; ++r) {
			for (int s = 0; s < segments; ++s) {
				uint32_t i0 = idx(r, s);
				uint32_t i1 = idx(r + 1, s);
				uint32_t i2 = idx(r + 1, s + 1);
				uint32_t i3 = idx(r, s + 1);

				mesh.primitives.emplace_back(
					static_cast <index_t> (i0),
					static_cast <index_t> (i2),
					static_cast <index_t> (i1)
				);
				mesh.primitives.emplace_back(
					static_cast <index_t> (i0),
					static_cast <index_t> (i3),
					static_cast <index_t> (i2)
				);
			}
		}

		return mesh;
	}

	static Mesh ico_sphere(float radius = 1.0f, int subdivisions = 2) {
		Mesh mesh;

		const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
		std::vector <glm::vec3> verts = {
			{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
			{ 0, -1, t }, { 0, 1, t }, { 0, -1, -t }, { 0, 1, -t },
			{ t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 }
		};

		for (auto &v : verts) v = glm::normalize(v);

		std::vector <std::array <uint32_t, 3>> faces = {
			{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
			{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
			{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
			{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
		};

		auto midpoint = [&](uint32_t a, uint32_t b, auto &cache) -> uint32_t {
			uint64_t key = (uint64_t(std::min(a,b)) << 32) | std::max(a,b);
			if (auto it = cache.find(key); it != cache.end())
				return it->second;

			glm::vec3 m = glm::normalize((verts[a] + verts[b]) * 0.5f);
			verts.push_back(m);
			uint32_t idx = static_cast <uint32_t> (verts.size() - 1);
			cache[key] = idx;
			return idx;
		};

		for (int i = 0; i < subdivisions; ++i) {
			std::unordered_map <uint64_t, uint32_t> cache;
			std::vector <std::array <uint32_t, 3>> nf;
			nf.reserve(faces.size() * 4);

			for (auto f : faces) {
				uint32_t a = midpoint(f[0], f[1], cache);
				uint32_t b = midpoint(f[1], f[2], cache);
				uint32_t c = midpoint(f[2], f[0], cache);

				nf.push_back({ f[0], a, c });
				nf.push_back({ f[1], b, a });
				nf.push_back({ f[2], c, b });
				nf.push_back({ a, b, c });
			}
			faces.swap(nf);
		}

		for (const auto &v : verts) {
			glm::vec3 p = radius * v;
			mesh.positions.emplace_back(p.x, p.y, p.z);
			if constexpr (F.normals != encodings::S2::Disable)
				mesh.normals.emplace_back(v.x, v.y, v.z);
			if constexpr (F.uvs != encodings::R2::Disable) {
				float u = (std::atan2(v.z, v.x) / glm::two_pi <float> ()) + 0.5f;
				float vv = 0.5f - std::asin(v.y) / glm::pi <float> ();
				mesh.uvs.emplace_back(u, vv);
			}
		}

		for (auto f : faces) {
			mesh.primitives.emplace_back(
				static_cast <index_t> (f[0]),
				static_cast <index_t> (f[1]),
				static_cast <index_t> (f[2])
			);
		}

		return mesh;
	}

	static Mesh cylinder(float radius = 1.0f, float height = 1.0f, int slices = 32, int stacks = 1, bool caps = true) {
		Mesh mesh;
		slices = std::max(3, slices);
		stacks = std::max(1, stacks);

		float half_h = 0.5f * height;

		// side vertices
		for (int y = 0; y <= stacks; ++y) {
			float t = float(y) / stacks;
			float h = glm::mix(-half_h, half_h, t);
			for (int s = 0; s <= slices; ++s) {
				float u = float(s) / slices;
				float ang = u * (2.0f * glm::pi<float>());
				float c = std::cos(ang), sng = std::sin(ang);

				glm::vec3 n = { c, 0, sng };
				glm::vec3 p = { radius * c, h, radius * sng };

				mesh.positions.emplace_back(p.x, p.y, p.z);
				if constexpr (F.normals != encodings::S2::Disable)
					mesh.normals.emplace_back(n.x, n.y, n.z);
				if constexpr (F.uvs != encodings::R2::Disable)
					mesh.uvs.emplace_back(u, t);
			}
		}

		auto side_idx = [=](int y, int s) {
			return y * (slices + 1) + s;
		};

		for (int y = 0; y < stacks; ++y) {
			for (int s = 0; s < slices; ++s) {
				uint32_t i0 = side_idx(y, s);
				uint32_t i1 = side_idx(y + 1, s);
				uint32_t i2 = side_idx(y + 1, s + 1);
				uint32_t i3 = side_idx(y, s + 1);
				mesh.primitives.emplace_back(
					static_cast <index_t> (i0),
					static_cast <index_t> (i1),
					static_cast <index_t> (i2)
				);
				mesh.primitives.emplace_back(
					static_cast <index_t> (i0),
					static_cast <index_t> (i2),
					static_cast <index_t> (i3)
				);
			}
		}

		if (caps) {
			auto add_cap = [&](bool top) {
				uint32_t center_index = static_cast <uint32_t> (mesh.positions.size());
				float y = top ? half_h : -half_h;
				glm::vec3 n = { 0, top ? 1.0f : -1.0f, 0 };
				mesh.positions.emplace_back(0, y, 0);
				if constexpr (F.normals != encodings::S2::Disable)
					mesh.normals.emplace_back(n.x, n.y, n.z);
				if constexpr (F.uvs != encodings::R2::Disable)
					mesh.uvs.emplace_back(0.5f, 0.5f);

				for (int s = 0; s <= slices; ++s) {
					float u = float(s) / slices;
					float ang = u * (2.0f * glm::pi<float>());
					float c = std::cos(ang), sng = std::sin(ang);
					glm::vec3 p = { radius * c, y, radius * sng };
					mesh.positions.emplace_back(p.x, p.y, p.z);
					if constexpr (F.normals != encodings::S2::Disable)
						mesh.normals.emplace_back(n.x, n.y, n.z);
					if constexpr (F.uvs != encodings::R2::Disable)
						mesh.uvs.emplace_back(0.5f * (c + 1.0f), 0.5f * (sng + 1.0f));
				}

				uint32_t ring_base = center_index + 1;
				for (int s = 0; s < slices; ++s) {
					uint32_t a = ring_base + s;
					uint32_t b = ring_base + s + 1;
					if (top)
						mesh.primitives.emplace_back(
							static_cast <index_t> (center_index),
							static_cast <index_t> (a),
							static_cast <index_t> (b)
						);
					else
						mesh.primitives.emplace_back(
							static_cast <index_t> (center_index),
							static_cast <index_t> (b),
							static_cast <index_t> (a)
						);
				}
			};

			add_cap(true);
			add_cap(false);
		}

		return mesh;
	}
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

	size_t size_bytes() const {
		size_t result = 0;
		for (const auto &mesh : meshes)
			result += mesh.size_bytes();
		return result;
	}

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
