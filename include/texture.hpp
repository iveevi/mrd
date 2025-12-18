#pragma once

#include <vector>
#include <string>
#include <expected>
#include <filesystem>
#include <unordered_map>
#include <cstddef>

#include <glm/vec2.hpp>

#include "encodings.hpp"
#include "representations.hpp"

namespace mrd {

// Forward declarations
template <TextureEncoding E>
struct Texture;

using CanonicalLDR = Texture <TextureEncoding::RGBA_UNorm8>;
using CanonicalHDR = Texture <TextureEncoding::RGBA_Srgb32>;

struct TextureLoadOptions {
	bool force_srgb = true;
	bool flip_vertical = false;
	bool generate_mips = false;
	int max_size = 16384;
};

struct TextureError {
	enum class Code {
		IOError,
		StbFailure,
		SizeOutOfRange,
		EmptyImage,
		UnsupportedEncoding,
	} code;

	std::string message;
};

namespace impl {

std::expected <CanonicalLDR, TextureError> load_canonical_ldr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
);

std::expected <CanonicalHDR, TextureError> load_canonical_hdr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
);

bool is_hdr_image(const std::filesystem::path &path);

} // namespace impl

// TODO: same deal here, we can just plug Es...
template <TextureEncoding E>
struct TextureCache {
	// TODO: read, write methods with optional lossless compression schemes (e.g. Huffman)
	std::unordered_map <
		std::filesystem::path,
		Texture <E>
	> entries;

	// TODO: TextureCacheOf <Texture <Es...>> if we go that route
};

template <TextureEncoding E>
struct Texture {
	struct Info {
		glm::ivec2 size { 0, 0 };
		bool is_srgb = true;
		bool mipmapped = false;
	};

	Info info;
	std::vector <encoding_representation_t <E>> data;
	std::vector <size_t> mip_offsets;

	template <TextureEncoding E2>
	Texture <E2> convert() const {
		Texture <E2> out;
		out.info.size = info.size;
		out.info.is_srgb = info.is_srgb;
		out.info.mipmapped = info.mipmapped;
		out.mip_offsets = { 0 };
		out.data.resize(data.size());

		for (size_t i = 0; i < data.size(); ++i)
			out.data[i] = static_cast <encoding_representation_t <E2>> (data[i]);

		return out;
	}

	// Loading methods
	static std::expected <Texture, TextureError> load(
		const std::filesystem::path &path,
		const TextureLoadOptions &options = {}
	) {
		if (path.extension() == ".exr") {
			TextureError err;
			err.code = TextureError::Code::UnsupportedEncoding;
			err.message = "EXR loading is not implemented yet";
			return std::unexpected(err);
		}

		const bool hdr = impl::is_hdr_image(path);

		if (hdr) {
			auto canonical = impl::load_canonical_hdr(path, options);
			if (!canonical)
				return std::unexpected(canonical.error());

			if constexpr (E == TextureEncoding::RGBA_Srgb32)
				return canonical;
			else
				return canonical->template convert <E> ();
		} else {
			auto canonical = impl::load_canonical_ldr(path, options);
			if (!canonical)
				return std::unexpected(canonical.error());

			if constexpr (E == TextureEncoding::RGBA_UNorm8)
				return canonical;
			else
				return canonical->template convert <E> ();
		}
	}

	static std::expected <Texture, TextureError> load(
		const std::filesystem::path &path,
		TextureCache <E> &cache,
		const TextureLoadOptions &options = {}
	) {
		if (auto it = cache.entries.find(path); it != cache.entries.end())
			return it->second;

		auto tex = Texture::load(path, options);
		if (tex)
			cache.entries.emplace(path, *tex);

		return tex;
	}
};

} // namespace mrd
