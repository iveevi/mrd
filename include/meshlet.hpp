#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "mesh.hpp"

namespace mrd {

struct Meshlet {
	uint32_t vertex_offset;
	uint32_t vertex_count;
	uint32_t prim_offset;
	uint32_t prim_count;
	glm::vec4 bounds;
};

struct MeshletBuild {
	std::vector <glm::vec4> positions;
	std::vector <glm::uvec3> triangles;
	std::vector <uint32_t> meshlet_vertices;
	std::vector <glm::uvec3> meshlet_triangles;
	std::vector <Meshlet> meshlets;
};

enum class MeshletBuildAlgorithm {
	eGreedyFrontier,
};

struct MeshletBuildOptions {
	MeshletBuildAlgorithm algorithm = MeshletBuildAlgorithm::eGreedyFrontier;
	uint32_t max_vertices = 64;
	uint32_t max_prims = 126;
	std::function <void (size_t done, size_t total)> progress;
};

inline auto build_meshlets(
	const Mesh &mesh,
	const MeshletBuildOptions &options = {}
) -> MeshletBuild
{
	auto out = MeshletBuild {};
	if (mesh.position != R3::Float3_32b)
		return out;
	if (mesh.connectivity != Connectivity::Triangle_UInt3_32b
	 && mesh.connectivity != Connectivity::Triangle_Int3_32b)
		return out;

	const uint32_t max_vertices = options.max_vertices;
	const uint32_t max_prims = std::min <uint32_t> (options.max_prims, max_vertices * 2);

	out.positions.reserve(mesh.position_count());
	out.triangles.reserve(mesh.primitive_count());

	for (const auto &p : mesh.positions_as <const float3_32b> ())
		out.positions.emplace_back(p.x, p.y, p.z, 1.0f);

	if (mesh.connectivity == Connectivity::Triangle_UInt3_32b) {
		for (const auto &tri : mesh.primitives_as <const uint3_32b> ())
			out.triangles.emplace_back(tri.x, tri.y, tri.z);
	} else {
		for (const auto &tri : mesh.primitives_as <const int3_32b> ()) {
			if (tri.x < 0 || tri.y < 0 || tri.z < 0)
				continue;
			out.triangles.emplace_back(uint32_t(tri.x), uint32_t(tri.y), uint32_t(tri.z));
		}
	}

	const size_t tri_count = out.triangles.size();

	struct TriInfo {
		glm::vec3 centroid;
		glm::vec3 normal;
		std::array <uint32_t, 3> v;
		std::vector <uint32_t> neighbors;
	};

	auto tri_info = std::vector <TriInfo> (tri_count);
	for (size_t i = 0; i < tri_count; ++i) {
		const auto &tri = out.triangles[i];
		auto p0 = glm::vec3(out.positions[tri.x]);
		auto p1 = glm::vec3(out.positions[tri.y]);
		auto p2 = glm::vec3(out.positions[tri.z]);
		auto n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
		if (glm::length(n) < 1e-6f)
			n = glm::vec3(0.0f, 1.0f, 0.0f);
		tri_info[i].centroid = (p0 + p1 + p2) / 3.0f;
		tri_info[i].normal = n;
		tri_info[i].v = { tri.x, tri.y, tri.z };
	}

	auto edge_map = std::unordered_map <uint64_t, uint32_t> ();
	edge_map.reserve(tri_count * 3);
	auto edge_key = [](uint32_t a, uint32_t b) -> uint64_t {
		uint64_t lo = std::min(a, b);
		uint64_t hi = std::max(a, b);
		return (hi << 32) | lo;
	};

	for (uint32_t i = 0; i < tri_count; ++i) {
		const auto &v = tri_info[i].v;
		for (int e = 0; e < 3; ++e) {
			uint32_t a = v[e];
			uint32_t b = v[(e + 1) % 3];
			auto key = edge_key(a, b);
			auto it = edge_map.find(key);
			if (it == edge_map.end()) {
				edge_map.emplace(key, i);
			} else {
				uint32_t other = it->second;
				tri_info[i].neighbors.push_back(other);
				tri_info[other].neighbors.push_back(i);
			}
		}
	}

	auto local_vertices = std::vector <uint32_t> ();
	auto local_tris = std::vector <glm::uvec3> ();
	auto local_map = std::unordered_map <uint32_t, uint32_t> ();

	auto flush_meshlet = [&]() {
		if (local_vertices.empty() || local_tris.empty())
			return;

		auto meshlet = Meshlet {};
		meshlet.vertex_offset = static_cast <uint32_t> (out.meshlet_vertices.size());
		meshlet.vertex_count = static_cast <uint32_t> (local_vertices.size());
		meshlet.prim_offset = static_cast <uint32_t> (out.meshlet_triangles.size());
		meshlet.prim_count = static_cast <uint32_t> (local_tris.size());

		glm::vec3 center(0.0f);
		for (auto idx : local_vertices)
			center += glm::vec3(out.positions[idx]);
		center /= float(local_vertices.size());

		float radius = 0.0f;
		for (auto idx : local_vertices) {
			glm::vec3 p = glm::vec3(out.positions[idx]);
			radius = std::max(radius, glm::length(p - center));
		}

		meshlet.bounds = glm::vec4(center, radius);

		out.meshlet_vertices.insert(out.meshlet_vertices.end(),
			local_vertices.begin(), local_vertices.end());
		out.meshlet_triangles.insert(out.meshlet_triangles.end(),
			local_tris.begin(), local_tris.end());
		out.meshlets.push_back(meshlet);
	};

	auto can_add_triangle = [&](uint32_t tri_idx) {
		uint32_t new_vertices = 0;
		for (uint32_t v : tri_info[tri_idx].v) {
			if (local_map.find(v) == local_map.end())
				new_vertices++;
		}
		if (local_vertices.size() + new_vertices > max_vertices)
			return false;
		if (local_tris.size() + 1 > max_prims)
			return false;
		return true;
	};

	auto add_triangle = [&](uint32_t tri_idx) {
		glm::uvec3 local {};
		for (int i = 0; i < 3; ++i) {
			uint32_t idx = tri_info[tri_idx].v[i];
			auto it = local_map.find(idx);
			if (it == local_map.end()) {
				uint32_t local_idx = static_cast <uint32_t> (local_vertices.size());
				local_vertices.push_back(idx);
				local_map.emplace(idx, local_idx);
				local[i] = local_idx;
			} else {
				local[i] = it->second;
			}
		}
		local_tris.push_back(local);
	};

	auto assigned = std::vector <uint8_t> (tri_count, 0);
	size_t assigned_count = 0;

	for (uint32_t seed = 0; seed < tri_count; ++seed) {
		if (assigned[seed])
			continue;

		local_vertices.clear();
		local_tris.clear();
		local_map.clear();

		add_triangle(seed);
		assigned[seed] = 1;
		assigned_count++;

		glm::vec3 centroid_acc = tri_info[seed].centroid;
		glm::vec3 normal_acc = tri_info[seed].normal;

		auto frontier = std::unordered_set <uint32_t> ();
		for (uint32_t n : tri_info[seed].neighbors)
			if (!assigned[n])
				frontier.insert(n);

		while (!frontier.empty()) {
			uint32_t best = UINT32_MAX;
			float best_score = -1e30f;

			for (uint32_t cand : frontier) {
				if (assigned[cand])
					continue;
				if (!can_add_triangle(cand))
					continue;

				uint32_t reuse = 0;
				for (uint32_t v : tri_info[cand].v)
					if (local_map.find(v) != local_map.end())
						reuse++;

				glm::vec3 cluster_center = centroid_acc / float(local_tris.size());
				float dist = glm::length(tri_info[cand].centroid - cluster_center);
				float normal_sim = glm::dot(tri_info[cand].normal,
					glm::normalize(normal_acc));

				float score = float(reuse) * 10.0f - dist * 0.5f + normal_sim * 2.0f;
				if (score > best_score) {
					best_score = score;
					best = cand;
				}
			}

			if (best == UINT32_MAX)
				break;

			frontier.erase(best);
			add_triangle(best);
			assigned[best] = 1;
			assigned_count++;
			centroid_acc += tri_info[best].centroid;
			normal_acc += tri_info[best].normal;

			for (uint32_t n : tri_info[best].neighbors)
				if (!assigned[n])
					frontier.insert(n);
		}

		flush_meshlet();

		if (options.progress)
			options.progress(assigned_count, tri_count);
	}

	return out;
}

} // namespace mrd
