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

enum class TextureEncoding {
	RGBA_UNorm8,
	RGBA_Srgb32,
	RGBA_Sint32,
	RGBA_Uint32,
};

struct MeshEncodings {
	Connectivity connectivity = Connectivity::Triangle_UInt3_32b;
	R3 positions = R3::Float3_32b;
	S2 normals = S2::Float3_32b;
	R2 uvs = R2::Float2_32b;
};

} // namespace mrd
