#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "encodings.hpp"

namespace mrd {

template <typename T>
struct basic_vector_space_d2 : public T {
	basic_vector_space_d2() = default;
	basic_vector_space_d2(auto x, auto y) : T(x, y) {}
	basic_vector_space_d2(const T &other) : T(other) {}

	friend T operator+(const T &a, const T &b) {
		return T { a.x + b.x, a.y + b.y };
	}

	template <typename U>
	explicit operator U() const {
		U v;
		v.x = static_cast <decltype(v.x)> (this->x);
		v.y = static_cast <decltype(v.y)> (this->y);
		return v;
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

	template <typename U>
	explicit operator U() const {
		U v;
		v.x = static_cast <decltype(v.x)> (this->x);
		v.y = static_cast <decltype(v.y)> (this->y);
		v.z = static_cast <decltype(v.z)> (this->z);
		return v;
	}
};

template <typename T>
struct basic_vector_space_d4 : public T {
	basic_vector_space_d4() = default;
	basic_vector_space_d4(auto x, auto y, auto z, auto w) : T(x, y, z, w) {}
	basic_vector_space_d4(const T &other) : T(other) {}

	template <typename U>
	explicit operator U() const {
		U v;
		v.x = static_cast <decltype(v.x)> (this->x);
		v.y = static_cast <decltype(v.y)> (this->y);
		v.z = static_cast <decltype(v.z)> (this->z);
		v.w = static_cast <decltype(v.w)> (this->w);
		return v;
	}
};

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

struct int4_32b {
	glm::int32_t x;
	glm::int32_t y;
	glm::int32_t z;
	glm::int32_t w;
};

struct uint4_32b {
	glm::uint32_t x;
	glm::uint32_t y;
	glm::uint32_t z;
	glm::uint32_t w;
};

struct uint4_8b {
	uint8_t x;
	uint8_t y;
	uint8_t z;
	uint8_t w;
};

} // namespace representations

using int3_32b = basic_vector_space_d3 <representations::int3_32b>;
using uint3_32b = basic_vector_space_d3 <representations::uint3_32b>;

using float2_32b = basic_vector_space_d2 <representations::float2_32b>;
using float3_32b = basic_vector_space_d3 <representations::float3_32b>;
using float4_32b = basic_vector_space_d4 <representations::float4_32b>;
using int4_32b = basic_vector_space_d4 <representations::int4_32b>;
using uint4_32b = basic_vector_space_d4 <representations::uint4_32b>;
using uint4_8b = basic_vector_space_d4 <representations::uint4_8b>;

template <auto E>
struct encoding_representation {};

#define ENCODING_REPRESENTATION(E, T) template <> struct encoding_representation <E> { using type = T; };

ENCODING_REPRESENTATION(R2::Float2_32b, float2_32b);
ENCODING_REPRESENTATION(R3::Float3_32b, float3_32b);

ENCODING_REPRESENTATION(S2::Float3_32b, float3_32b);
ENCODING_REPRESENTATION(S2::Disable, std::nullptr_t);

ENCODING_REPRESENTATION(Connectivity::Triangle_Int3_32b, int3_32b);
ENCODING_REPRESENTATION(Connectivity::Triangle_UInt3_32b, uint3_32b);

ENCODING_REPRESENTATION(Pixel::RGBA_UNorm8, uint4_8b);
ENCODING_REPRESENTATION(Pixel::RGBA_Srgb32, float4_32b);
ENCODING_REPRESENTATION(Pixel::RGBA_Sint32, int4_32b);
ENCODING_REPRESENTATION(Pixel::RGBA_Uint32, uint4_32b);

template <auto E>
using encoding_representation_t = typename encoding_representation <E> ::type;

} // namespace mrd
