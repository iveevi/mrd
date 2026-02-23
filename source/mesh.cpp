#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>

#include "mesh.hpp"

namespace mrd {

size_t Mesh::position_count() const
{
	auto stride = position_stride();
	if (stride == 0)
		return 0;
	return positions.size() / stride;
}

size_t Mesh::normal_count() const
{
	auto stride = normal_stride();
	if (stride == 0)
		return 0;
	return normals.size() / stride;
}

size_t Mesh::uv_count() const
{
	auto stride = uv_stride();
	if (stride == 0)
		return 0;
	return uvs.size() / stride;
}

size_t Mesh::primitive_count() const
{
	auto stride = primitive_stride();
	if (stride == 0)
		return 0;
	return primitives.size() / stride;
}

size_t Mesh::size_bytes() const
{
	return sizeof(transform)
		+ positions.size()
		+ normals.size()
		+ uvs.size()
		+ primitives.size();
}

void Mesh::deduplicate()
{
	if (position != R3::Float3_32b)
		return;
	if (normal != S2::Disable && normal != S2::Float3_32b)
		return;
	if (uv != R2::Disable && uv != R2::Float2_32b)
		return;
	if (connectivity != Connectivity::Triangle_UInt3_32b
	 && connectivity != Connectivity::Triangle_Int3_32b)
		return;

	const auto source_positions = positions_as <const float3_32b> ();
	if (source_positions.empty())
		return;

	auto hash = [](const float3_32b &p) noexcept {
		return float3_32b::hash(p);
	};

	auto eq = [](const float3_32b &a, const float3_32b &b) noexcept {
		return a == b;
	};

	std::unordered_map <float3_32b, size_t, decltype(hash), decltype(eq)> remap(
		source_positions.size(), hash, eq
	);

	auto new_positions = std::vector <float3_32b> ();
	new_positions.reserve(source_positions.size());

	auto source_normals = std::span <const float3_32b> ();
	auto new_normals = std::vector <float3_32b> ();
	if (normal == S2::Float3_32b) {
		source_normals = normals_as <const float3_32b> ();
		new_normals.reserve(source_normals.size());
	}

	auto source_uvs = std::span <const float2_32b> ();
	auto new_uvs = std::vector <float2_32b> ();
	if (uv == R2::Float2_32b) {
		source_uvs = uvs_as <const float2_32b> ();
		new_uvs.reserve(source_uvs.size());
	}

	auto get_or_insert = [&](size_t idx) -> size_t {
		const auto &p = source_positions[idx];
		auto [it, inserted] = remap.try_emplace(p, new_positions.size());
		if (inserted) {
			new_positions.push_back(p);
			if (!source_normals.empty() && idx < source_normals.size())
				new_normals.push_back(source_normals[idx]);
			if (!source_uvs.empty() && idx < source_uvs.size())
				new_uvs.push_back(source_uvs[idx]);
		}
		return it->second;
	};

	if (connectivity == Connectivity::Triangle_UInt3_32b) {
		auto tris = primitives_as <uint3_32b> ();
		for (auto &tri : tris) {
			tri.x = static_cast <decltype(tri.x)> (get_or_insert(tri.x));
			tri.y = static_cast <decltype(tri.y)> (get_or_insert(tri.y));
			tri.z = static_cast <decltype(tri.z)> (get_or_insert(tri.z));
		}
	} else {
		auto tris = primitives_as <int3_32b> ();
		for (auto &tri : tris) {
			tri.x = static_cast <decltype(tri.x)> (get_or_insert(size_t(tri.x)));
			tri.y = static_cast <decltype(tri.y)> (get_or_insert(size_t(tri.y)));
			tri.z = static_cast <decltype(tri.z)> (get_or_insert(size_t(tri.z)));
		}
	}

	positions.resize(std::span(new_positions).size_bytes());
	std::copy(
		new_positions.begin(),
		new_positions.end(),
		positions_as <float3_32b> ().begin()
	);

	if (normal == S2::Float3_32b)
	{
		normals.resize(std::span(new_normals).size_bytes());
		std::copy(
			new_normals.begin(),
			new_normals.end(),
			normals_as <float3_32b> ().begin()
		);
	}
	if (uv == R2::Float2_32b)
	{
		uvs.resize(std::span(new_uvs).size_bytes());
		std::copy(
			new_uvs.begin(),
			new_uvs.end(),
			uvs_as <float2_32b> ().begin()
		);
	}
}

void Mesh::recalculate_normals()
{
	if (position != R3::Float3_32b)
		return;
	if (normal != S2::Float3_32b)
		return;
	if (connectivity != Connectivity::Triangle_UInt3_32b
	 && connectivity != Connectivity::Triangle_Int3_32b)
		return;

	auto source_positions = positions_as <const float3_32b> ();
	auto result_normals = std::vector <float3_32b> (source_positions.size(), float3_32b(0.0f, 0.0f, 0.0f));
	if (source_positions.empty()) {
		normals.resize(std::span(result_normals).size_bytes());
		std::copy(
			result_normals.begin(),
			result_normals.end(),
			normals_as <float3_32b> ().begin()
		);
		return;
	}

	auto accum = std::vector <glm::vec3> (source_positions.size(), glm::vec3(0.0f));

	auto add_triangle = [&](size_t i0, size_t i1, size_t i2) {
		if (i0 >= source_positions.size()
		 || i1 >= source_positions.size()
		 || i2 >= source_positions.size())
			return;

		glm::vec3 p0 {
			source_positions[i0].x,
			source_positions[i0].y,
			source_positions[i0].z,
		};
		glm::vec3 p1 {
			source_positions[i1].x,
			source_positions[i1].y,
			source_positions[i1].z,
		};
		glm::vec3 p2 {
			source_positions[i2].x,
			source_positions[i2].y,
			source_positions[i2].z,
		};

		auto n = glm::cross(p1 - p0, p2 - p0);
		if (glm::dot(n, n) == 0.0f)
			return;
		n = glm::normalize(n);

		accum[i0] += n;
		accum[i1] += n;
		accum[i2] += n;
	};

	if (connectivity == Connectivity::Triangle_UInt3_32b) {
		for (const auto &tri : primitives_as <const uint3_32b> ())
			add_triangle(size_t(tri.x), size_t(tri.y), size_t(tri.z));
	} else {
		for (const auto &tri : primitives_as <const int3_32b> ()) {
			if (tri.x < 0 || tri.y < 0 || tri.z < 0)
				continue;
			add_triangle(size_t(tri.x), size_t(tri.y), size_t(tri.z));
		}
	}

	for (size_t i = 0; i < accum.size(); ++i) {
		auto n = accum[i];
		if (glm::dot(n, n) == 0.0f)
			n = glm::vec3(0.0f, 1.0f, 0.0f);
		else
			n = glm::normalize(n);

		result_normals[i] = float3_32b(n.x, n.y, n.z);
	}

	normals.resize(std::span(result_normals).size_bytes());
	std::copy(
		result_normals.begin(),
		result_normals.end(),
		normals_as <float3_32b> ().begin()
	);
}

auto Mesh::box(glm::vec3 extent) -> Mesh
{
	auto mesh = Mesh {};

	auto positions = std::vector <float3_32b> ();
	auto normals = std::vector <float3_32b> ();
	auto uvs = std::vector <float2_32b> ();
	auto primitives = std::vector <uint3_32b> ();

	const glm::vec3 he = 0.5f * extent;

	auto push = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uvv) {
		positions.emplace_back(p.x, p.y, p.z);
		normals.emplace_back(n.x, n.y, n.z);
		uvs.emplace_back(uvv.x, uvv.y);
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

		size_t base = positions.size();

		push(v0, n, { 0, 0 });
		push(v1, n, { 1, 0 });
		push(v2, n, { 1, 1 });
		push(v3, n, { 0, 1 });

		primitives.emplace_back(
			static_cast <uint32_t> (base + 0),
			static_cast <uint32_t> (base + 1),
			static_cast <uint32_t> (base + 2)
		);
		primitives.emplace_back(
			static_cast <uint32_t> (base + 0),
			static_cast <uint32_t> (base + 2),
			static_cast <uint32_t> (base + 3)
		);
	};

	add_face({ 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 });
	add_face({ -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 });
	add_face({ 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 });
	add_face({ 0, -1, 0 }, { 0, 0, -1 }, { 1, 0, 0 });
	add_face({ 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 });
	add_face({ 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 });

	mesh.positions.resize(std::span(positions).size_bytes());
	std::copy(
		positions.begin(),
		positions.end(),
		mesh.positions_as <float3_32b> ().begin()
	);

	mesh.normals.resize(std::span(normals).size_bytes());
	std::copy(
		normals.begin(),
		normals.end(),
		mesh.normals_as <float3_32b> ().begin()
	);

	mesh.uvs.resize(std::span(uvs).size_bytes());
	std::copy(
		uvs.begin(),
		uvs.end(),
		mesh.uvs_as <float2_32b> ().begin()
	);

	mesh.primitives.resize(std::span(primitives).size_bytes());
	std::copy(
		primitives.begin(),
		primitives.end(),
		mesh.primitives_as <uint3_32b> ().begin()
	);
	return mesh;
}

auto Mesh::uv_sphere(float radius, int rings, int segments) -> Mesh
{
	auto mesh = Mesh {};

	auto positions = std::vector <float3_32b> ();
	auto normals = std::vector <float3_32b> ();
	auto uvs = std::vector <float2_32b> ();
	auto primitives = std::vector <uint3_32b> ();

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

			positions.emplace_back(p.x, p.y, p.z);
			normals.emplace_back(n.x, n.y, n.z);
			uvs.emplace_back(u, 1.0f - v);
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

			primitives.emplace_back(i0, i2, i1);
			primitives.emplace_back(i0, i3, i2);
		}
	}

	mesh.positions.resize(std::span(positions).size_bytes());
	std::copy(
		positions.begin(),
		positions.end(),
		mesh.positions_as <float3_32b> ().begin()
	);

	mesh.normals.resize(std::span(normals).size_bytes());
	std::copy(
		normals.begin(),
		normals.end(),
		mesh.normals_as <float3_32b> ().begin()
	);

	mesh.uvs.resize(std::span(uvs).size_bytes());
	std::copy(
		uvs.begin(),
		uvs.end(),
		mesh.uvs_as <float2_32b> ().begin()
	);

	mesh.primitives.resize(std::span(primitives).size_bytes());
	std::copy(
		primitives.begin(),
		primitives.end(),
		mesh.primitives_as <uint3_32b> ().begin()
	);
	return mesh;
}

auto Mesh::ico_sphere(float radius, int subdivisions) -> Mesh
{
	auto mesh = Mesh {};

	auto positions = std::vector <float3_32b> ();
	auto normals = std::vector <float3_32b> ();
	auto uvs = std::vector <float2_32b> ();
	auto primitives = std::vector <uint3_32b> ();

	const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
	std::vector <glm::vec3> verts = {
		{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
		{ 0, -1, t }, { 0, 1, t }, { 0, -1, -t }, { 0, 1, -t },
		{ t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 }
	};

	for (auto &v : verts)
		v = glm::normalize(v);

	std::vector <std::array <uint32_t, 3>> faces = {
		{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
		{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
		{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
		{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
	};

	auto midpoint = [&](uint32_t a, uint32_t b, auto &cache) -> uint32_t {
		uint64_t key = (uint64_t(std::min(a, b)) << 32) | std::max(a, b);
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
		positions.emplace_back(p.x, p.y, p.z);
		normals.emplace_back(v.x, v.y, v.z);

		float u = (std::atan2(v.z, v.x) / glm::two_pi <float> ()) + 0.5f;
		float vv = 0.5f - std::asin(v.y) / glm::pi <float> ();
		uvs.emplace_back(u, vv);
	}

	for (auto f : faces)
		primitives.emplace_back(f[0], f[1], f[2]);

	mesh.positions.resize(std::span(positions).size_bytes());
	std::copy(
		positions.begin(),
		positions.end(),
		mesh.positions_as <float3_32b> ().begin()
	);

	mesh.normals.resize(std::span(normals).size_bytes());
	std::copy(
		normals.begin(),
		normals.end(),
		mesh.normals_as <float3_32b> ().begin()
	);

	mesh.uvs.resize(std::span(uvs).size_bytes());
	std::copy(
		uvs.begin(),
		uvs.end(),
		mesh.uvs_as <float2_32b> ().begin()
	);

	mesh.primitives.resize(std::span(primitives).size_bytes());
	std::copy(
		primitives.begin(),
		primitives.end(),
		mesh.primitives_as <uint3_32b> ().begin()
	);
	return mesh;
}

auto Mesh::cylinder(float radius, float height, int slices, int stacks, bool caps) -> Mesh
{
	auto mesh = Mesh {};

	auto positions = std::vector <float3_32b> ();
	auto normals = std::vector <float3_32b> ();
	auto uvs = std::vector <float2_32b> ();
	auto primitives = std::vector <uint3_32b> ();

	slices = std::max(3, slices);
	stacks = std::max(1, stacks);

	float half_h = 0.5f * height;

	for (int y = 0; y <= stacks; ++y) {
		float t = float(y) / stacks;
		float h = glm::mix(-half_h, half_h, t);
		for (int s = 0; s <= slices; ++s) {
			float u = float(s) / slices;
			float ang = u * (2.0f * glm::pi <float> ());
			float c = std::cos(ang);
			float sng = std::sin(ang);

			glm::vec3 n = { c, 0, sng };
			glm::vec3 p = { radius * c, h, radius * sng };

			positions.emplace_back(p.x, p.y, p.z);
			normals.emplace_back(n.x, n.y, n.z);
			uvs.emplace_back(u, t);
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
			primitives.emplace_back(i0, i1, i2);
			primitives.emplace_back(i0, i2, i3);
		}
	}

	if (caps) {
		auto add_cap = [&](bool top) {
			uint32_t center_index = static_cast <uint32_t> (positions.size());
			float y = top ? half_h : -half_h;
			glm::vec3 n = { 0, top ? 1.0f : -1.0f, 0 };
			positions.emplace_back(0.0f, y, 0.0f);
			normals.emplace_back(n.x, n.y, n.z);
			uvs.emplace_back(0.5f, 0.5f);

			for (int s = 0; s <= slices; ++s) {
				float u = float(s) / slices;
				float ang = u * (2.0f * glm::pi <float> ());
				float c = std::cos(ang);
				float sng = std::sin(ang);
				glm::vec3 p = { radius * c, y, radius * sng };
				positions.emplace_back(p.x, p.y, p.z);
				normals.emplace_back(n.x, n.y, n.z);
				uvs.emplace_back(0.5f * (c + 1.0f), 0.5f * (sng + 1.0f));
			}

			uint32_t ring_base = center_index + 1;
			for (int s = 0; s < slices; ++s) {
				uint32_t a = ring_base + s;
				uint32_t b = ring_base + s + 1;
				if (top)
					primitives.emplace_back(center_index, a, b);
				else
					primitives.emplace_back(center_index, b, a);
			}
		};

		add_cap(true);
		add_cap(false);
	}

	mesh.positions.resize(std::span(positions).size_bytes());
	std::copy(
		positions.begin(),
		positions.end(),
		mesh.positions_as <float3_32b> ().begin()
	);

	mesh.normals.resize(std::span(normals).size_bytes());
	std::copy(
		normals.begin(),
		normals.end(),
		mesh.normals_as <float3_32b> ().begin()
	);

	mesh.uvs.resize(std::span(uvs).size_bytes());
	std::copy(
		uvs.begin(),
		uvs.end(),
		mesh.uvs_as <float2_32b> ().begin()
	);

	mesh.primitives.resize(std::span(primitives).size_bytes());
	std::copy(
		primitives.begin(),
		primitives.end(),
		mesh.primitives_as <uint3_32b> ().begin()
	);
	return mesh;
}

auto merge_meshes(const std::span <const Mesh> meshes) -> Mesh
{
	auto merged = Mesh {};
	if (meshes.empty())
		return merged;

	merged.position = meshes.front().position;
	merged.normal = meshes.front().normal;
	merged.uv = meshes.front().uv;
	merged.connectivity = Connectivity::Triangle_UInt3_32b;

	if (merged.position != R3::Float3_32b)
		return merged;

	auto merged_positions = std::vector <float3_32b> ();
	auto merged_normals = std::vector <float3_32b> ();
	auto merged_uvs = std::vector <float2_32b> ();
	auto merged_primitives = std::vector <uint3_32b> ();

	for (const auto &mesh : meshes) {
		if (mesh.position != R3::Float3_32b)
			continue;

		auto src_positions = mesh.positions_as <const float3_32b> ();
		const uint32_t base = static_cast <uint32_t> (merged_positions.size());

		for (const auto &pos : src_positions) {
			auto p = glm::vec4(pos.x, pos.y, pos.z, 1.0f);
			auto wp = mesh.transform * p;
			merged_positions.emplace_back(wp.x, wp.y, wp.z);
		}

		if (merged.normal != S2::Disable) {
			if (mesh.normal == S2::Float3_32b) {
				auto src_normals = mesh.normals_as <const float3_32b> ();
				auto nmat = glm::mat3(mesh.transform);
				for (const auto &n : src_normals) {
					auto wn = glm::normalize(nmat * glm::vec3(n.x, n.y, n.z));
					merged_normals.emplace_back(wn.x, wn.y, wn.z);
				}
			} else {
				for (size_t i = 0; i < src_positions.size(); ++i)
					merged_normals.emplace_back(0.0f, 1.0f, 0.0f);
			}
		}

		if (merged.uv == R2::Float2_32b) {
			if (mesh.uv == R2::Float2_32b) {
				auto src_uvs = mesh.uvs_as <const float2_32b> ();
				merged_uvs.insert(merged_uvs.end(), src_uvs.begin(), src_uvs.end());
			} else {
				for (size_t i = 0; i < src_positions.size(); ++i)
					merged_uvs.emplace_back(0.0f, 0.0f);
			}
		}

		if (mesh.connectivity == Connectivity::Triangle_UInt3_32b) {
			for (const auto &tri : mesh.primitives_as <const uint3_32b> ()) {
				merged_primitives.emplace_back(
					tri.x + base,
					tri.y + base,
					tri.z + base
				);
			}
		} else if (mesh.connectivity == Connectivity::Triangle_Int3_32b) {
			for (const auto &tri : mesh.primitives_as <const int3_32b> ()) {
				if (tri.x < 0 || tri.y < 0 || tri.z < 0)
					continue;
				merged_primitives.emplace_back(
					uint32_t(tri.x) + base,
					uint32_t(tri.y) + base,
					uint32_t(tri.z) + base
				);
			}
		}
	}

	merged.positions.resize(std::span(merged_positions).size_bytes());
	std::copy(
		merged_positions.begin(),
		merged_positions.end(),
		merged.positions_as <float3_32b> ().begin()
	);
	if (merged.normal != S2::Disable)
	{
		merged.normals.resize(std::span(merged_normals).size_bytes());
		std::copy(
			merged_normals.begin(),
			merged_normals.end(),
			merged.normals_as <float3_32b> ().begin()
		);
	}
	if (merged.uv == R2::Float2_32b)
	{
		merged.uvs.resize(std::span(merged_uvs).size_bytes());
		std::copy(
			merged_uvs.begin(),
			merged_uvs.end(),
			merged.uvs_as <float2_32b> ().begin()
		);
	}
	merged.primitives.resize(std::span(merged_primitives).size_bytes());
	std::copy(
		merged_primitives.begin(),
		merged_primitives.end(),
		merged.primitives_as <uint3_32b> ().begin()
	);
	return merged;
}

} // namespace mrd
