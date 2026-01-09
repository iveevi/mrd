#pragma once

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

} // namespace mrd
