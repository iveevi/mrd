#pragma once

#include <cassert>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "encodings.hpp"
#include "representations.hpp"

namespace mrd {

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

struct TextureCache;

struct Texture {
	struct Description {
		glm::ivec2 size { 0, 0 };
		bool is_srgb = true;
		bool mipmapped = false;
	};

	Pixel pixel = Pixel::RGBA_UNorm8;
	Description desc;
	std::vector <std::byte> data;
	std::vector <size_t> mip_offsets;

	size_t pixel_stride() const { return encoding_size_bytes(pixel); }

	size_t pixel_count() const
	{
		auto stride = pixel_stride();
		if (stride == 0)
			return 0;
		return data.size() / stride;
	}

	template <typename T>
	auto pixels_as() -> std::span <T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(pixel_stride() == sizeof(T));
		assert((data.size() % sizeof(T)) == 0);
		return std::span <T> (
			reinterpret_cast <T *> (data.data()),
			data.size() / sizeof(T)
		);
	}

	template <typename T>
	auto pixels_as() const -> std::span <const T>
	{
		static_assert(std::is_trivially_copyable_v <T>);
		assert(pixel_stride() == sizeof(T));
		assert((data.size() % sizeof(T)) == 0);
		return std::span <const T> (
			reinterpret_cast <const T *> (data.data()),
			data.size() / sizeof(T)
		);
	}

	static auto black(Pixel pixel = Pixel::RGBA_UNorm8) -> Texture;
	auto convert(Pixel target_pixel) const -> Texture;

	static auto load(
		const std::filesystem::path &path,
		const TextureLoadOptions &options = {},
		Pixel pixel = Pixel::RGBA_UNorm8
	) -> std::expected <Texture, TextureError>;

	static auto load(
		const std::filesystem::path &path,
		TextureCache &cache,
		const TextureLoadOptions &options = {},
		Pixel pixel = Pixel::RGBA_UNorm8
	) -> std::expected <Texture, TextureError>;
};

struct TextureCache {
	std::unordered_map <std::filesystem::path, Texture> entries;
};

namespace impl {

std::expected <Texture, TextureError> load_canonical_ldr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
);

std::expected <Texture, TextureError> load_canonical_hdr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
);

bool is_hdr_image(const std::filesystem::path &path);

} // namespace impl

} // namespace mrd
