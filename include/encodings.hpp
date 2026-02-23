#pragma once

#include <cstddef>
#include <cstdint>

namespace mrd {

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

enum class Pixel {
	RGBA_UNorm8,
	RGBA_Srgb32,
	RGBA_Sint32,
	RGBA_Uint32,
};

constexpr size_t encoding_size_bytes(R3 encoding)
{
	switch (encoding) {
		case R3::Float3_32b:
			return 3 * sizeof(float);
		case R3::Float3_64b:
			return 3 * sizeof(double);
	}

	return 0;
}

constexpr size_t encoding_size_bytes(R2 encoding)
{
	switch (encoding) {
		case R2::Disable:
			return 0;
		case R2::Float2_32b:
			return 2 * sizeof(float);
	}

	return 0;
}

constexpr size_t encoding_size_bytes(S2 encoding)
{
	switch (encoding) {
		case S2::Disable:
			return 0;
		case S2::Float3_32b:
			return 3 * sizeof(float);
		case S2::Spherical_32b:
			return 2 * sizeof(float);
	}

	return 0;
}

constexpr size_t encoding_size_bytes(Connectivity encoding)
{
	switch (encoding) {
		case Connectivity::Triangle_Int3_32b:
		case Connectivity::Triangle_UInt3_32b:
			return 3 * sizeof(uint32_t);
	}

	return 0;
}

constexpr size_t encoding_size_bytes(Pixel encoding)
{
	switch (encoding) {
		case Pixel::RGBA_UNorm8:
			return 4 * sizeof(uint8_t);
		case Pixel::RGBA_Srgb32:
			return 4 * sizeof(float);
		case Pixel::RGBA_Sint32:
			return 4 * sizeof(int32_t);
		case Pixel::RGBA_Uint32:
			return 4 * sizeof(uint32_t);
	}

	return 0;
}

} // namespace mrd
