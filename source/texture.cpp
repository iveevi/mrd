#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT 0
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include "texture.hpp"

namespace mrd {

template <typename T>
static void assign_pixels(Texture &texture, const std::span <const T> values)
{
	texture.data.resize(values.size_bytes());
	std::copy(
		values.begin(),
		values.end(),
		texture.pixels_as <T> ().begin()
	);
}

auto Texture::black(Pixel target_pixel) -> Texture
{
	auto result = Texture {};
	result.pixel = target_pixel;
	result.desc.size = { 1, 1 };
	result.desc.is_srgb = (target_pixel == Pixel::RGBA_UNorm8 || target_pixel == Pixel::RGBA_Srgb32);
	result.desc.mipmapped = false;
	result.mip_offsets = { 0 };

	switch (target_pixel) {
		case Pixel::RGBA_UNorm8: {
			auto value = uint4_8b {};
			assign_pixels(result, std::span <const uint4_8b> (&value, 1));
			break;
		}
		case Pixel::RGBA_Srgb32: {
			auto value = float4_32b {};
			assign_pixels(result, std::span <const float4_32b> (&value, 1));
			break;
		}
		case Pixel::RGBA_Sint32: {
			auto value = int4_32b {};
			assign_pixels(result, std::span <const int4_32b> (&value, 1));
			break;
		}
		case Pixel::RGBA_Uint32: {
			auto value = uint4_32b {};
			assign_pixels(result, std::span <const uint4_32b> (&value, 1));
			break;
		}
	}

	return result;
}

auto Texture::convert(Pixel target_pixel) const -> Texture
{
	if (pixel == target_pixel)
		return *this;

	auto result = Texture {};
	result.pixel = target_pixel;
	result.desc = desc;
	result.mip_offsets = mip_offsets;

	const auto count = pixel_count();

	auto sample = [&](size_t i) -> glm::vec4 {
		switch (pixel) {
			case Pixel::RGBA_UNorm8: {
				auto src = pixels_as <const uint4_8b> ();
				return glm::vec4(
					float(src[i].x) / 255.0f,
					float(src[i].y) / 255.0f,
					float(src[i].z) / 255.0f,
					float(src[i].w) / 255.0f
				);
			}
			case Pixel::RGBA_Srgb32: {
				auto src = pixels_as <const float4_32b> ();
				return glm::vec4(src[i].x, src[i].y, src[i].z, src[i].w);
			}
			case Pixel::RGBA_Sint32: {
				auto src = pixels_as <const int4_32b> ();
				return glm::vec4(
					float(src[i].x),
					float(src[i].y),
					float(src[i].z),
					float(src[i].w)
				);
			}
			case Pixel::RGBA_Uint32: {
				auto src = pixels_as <const uint4_32b> ();
				return glm::vec4(
					float(src[i].x),
					float(src[i].y),
					float(src[i].z),
					float(src[i].w)
				);
			}
		}

		return glm::vec4(0.0f);
	};

	switch (target_pixel) {
		case Pixel::RGBA_UNorm8: {
			auto out = std::vector <uint4_8b> (count);
			for (size_t i = 0; i < count; ++i) {
				auto c = glm::clamp(sample(i), glm::vec4(0.0f), glm::vec4(1.0f));
				out[i] = uint4_8b(
					uint8_t(std::round(c.x * 255.0f)),
					uint8_t(std::round(c.y * 255.0f)),
					uint8_t(std::round(c.z * 255.0f)),
					uint8_t(std::round(c.w * 255.0f))
				);
			}
			assign_pixels(result, std::span <const uint4_8b> (out));
			result.desc.is_srgb = true;
			break;
		}
		case Pixel::RGBA_Srgb32: {
			auto out = std::vector <float4_32b> (count);
			for (size_t i = 0; i < count; ++i) {
				auto c = sample(i);
				out[i] = float4_32b(c.x, c.y, c.z, c.w);
			}
			assign_pixels(result, std::span <const float4_32b> (out));
			break;
		}
		case Pixel::RGBA_Sint32: {
			auto out = std::vector <int4_32b> (count);
			for (size_t i = 0; i < count; ++i) {
				auto c = sample(i);
				out[i] = int4_32b(
					int32_t(std::round(c.x)),
					int32_t(std::round(c.y)),
					int32_t(std::round(c.z)),
					int32_t(std::round(c.w))
				);
			}
			assign_pixels(result, std::span <const int4_32b> (out));
			result.desc.is_srgb = false;
			break;
		}
		case Pixel::RGBA_Uint32: {
			auto out = std::vector <uint4_32b> (count);
			for (size_t i = 0; i < count; ++i) {
				auto c = sample(i);
				out[i] = uint4_32b(
					uint32_t(std::max(0.0f, std::round(c.x))),
					uint32_t(std::max(0.0f, std::round(c.y))),
					uint32_t(std::max(0.0f, std::round(c.z))),
					uint32_t(std::max(0.0f, std::round(c.w)))
				);
			}
			assign_pixels(result, std::span <const uint4_32b> (out));
			result.desc.is_srgb = false;
			break;
		}
	}

	return result;
}

auto Texture::load(
	const std::filesystem::path &path,
	const TextureLoadOptions &options,
	Pixel target_pixel
) -> std::expected <Texture, TextureError>
{
	if (path.extension() == ".exr") {
		auto err = TextureError {};
		err.code = TextureError::Code::UnsupportedEncoding;
		err.message = "EXR loading is not implemented yet";
		return std::unexpected(err);
	}

	const bool hdr = impl::is_hdr_image(path);
	auto canonical = hdr
		? impl::load_canonical_hdr(path, options)
		: impl::load_canonical_ldr(path, options);

	if (!canonical)
		return std::unexpected(canonical.error());

	if (canonical->pixel == target_pixel)
		return canonical;

	return canonical->convert(target_pixel);
}

auto Texture::load(
	const std::filesystem::path &path,
	TextureCache &cache,
	const TextureLoadOptions &options,
	Pixel target_pixel
) -> std::expected <Texture, TextureError>
{
	if (auto it = cache.entries.find(path); it != cache.entries.end()) {
		if (it->second.pixel == target_pixel)
			return it->second;
		return it->second.convert(target_pixel);
	}

	auto tex = Texture::load(path, options, target_pixel);
	if (tex)
		cache.entries.emplace(path, *tex);

	return tex;
}

} // namespace mrd

namespace mrd::impl {

struct LoadedImage {
	int width = 0;
	int height = 0;
	int channels = 4;
	bool hdr = false;
	std::vector <uint8_t> data8;
	std::vector <float> dataf;
};

bool is_hdr_image(const std::filesystem::path &path)
{
	return stbi_is_hdr(path.string().c_str()) == 1;
}

static std::expected <LoadedImage, TextureError> load_with_stb(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
)
{
	stbi_set_flip_vertically_on_load(options.flip_vertical ? 1 : 0);

	auto img = LoadedImage {};

	const auto path_str = path.string();
	int w = 0;
	int h = 0;
	int channels = 0;

	img.hdr = stbi_is_hdr(path_str.c_str()) == 1;

	if (img.hdr) {
		float *pixels = stbi_loadf(path_str.c_str(), &w, &h, &channels, 4);
		if (!pixels)
			return std::unexpected(TextureError { TextureError::Code::StbFailure, stbi_failure_reason() });

		img.dataf.assign(pixels, pixels + static_cast <size_t> (w) * h * 4);
		stbi_image_free(pixels);
	} else {
		unsigned char *pixels = stbi_load(path_str.c_str(), &w, &h, &channels, 4);
		if (!pixels)
			return std::unexpected(TextureError { TextureError::Code::StbFailure, stbi_failure_reason() });

		img.data8.assign(pixels, pixels + static_cast <size_t> (w) * h * 4);
		stbi_image_free(pixels);
	}

	img.width = w;
	img.height = h;
	img.channels = 4;

	if (img.width == 0 || img.height == 0 || (img.data8.empty() && img.dataf.empty()))
		return std::unexpected(TextureError { TextureError::Code::EmptyImage, "empty image or load failure" });

	if (img.width > options.max_size || img.height > options.max_size) {
		return std::unexpected(TextureError {
			TextureError::Code::SizeOutOfRange,
			"image dimensions exceed TextureLoadOptions::max_size",
		});
	}

	if (options.generate_mips) {
		return std::unexpected(TextureError {
			TextureError::Code::UnsupportedEncoding,
			"mip generation is not implemented yet",
		});
	}

	return img;
}

std::expected <Texture, TextureError> load_canonical_ldr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
)
{
	auto loaded = load_with_stb(path, options);
	if (!loaded)
		return std::unexpected(loaded.error());

	if (loaded->hdr) {
		return std::unexpected(TextureError {
			TextureError::Code::UnsupportedEncoding,
			"requested LDR loader but image is HDR",
		});
	}

	auto tex = Texture {};
	tex.pixel = Pixel::RGBA_UNorm8;
	tex.desc.size = { loaded->width, loaded->height };
	tex.desc.is_srgb = true;
	tex.desc.mipmapped = false;
	tex.mip_offsets = { 0 };

	auto pixels = std::vector <uint4_8b> (loaded->data8.size() / 4);
	for (size_t i = 0, o = 0; i + 3 < loaded->data8.size(); i += 4, ++o) {
		pixels[o] = uint4_8b(
			loaded->data8[i + 0],
			loaded->data8[i + 1],
			loaded->data8[i + 2],
			loaded->data8[i + 3]
		);
	}

	assign_pixels(tex, std::span <const uint4_8b> (pixels));
	return tex;
}

std::expected <Texture, TextureError> load_canonical_hdr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
)
{
	auto loaded = load_with_stb(path, options);
	if (!loaded)
		return std::unexpected(loaded.error());

	if (!loaded->hdr) {
		return std::unexpected(TextureError {
			TextureError::Code::UnsupportedEncoding,
			"requested HDR loader but image is LDR",
		});
	}

	auto tex = Texture {};
	tex.pixel = Pixel::RGBA_Srgb32;
	tex.desc.size = { loaded->width, loaded->height };
	tex.desc.is_srgb = options.force_srgb;
	tex.desc.mipmapped = false;
	tex.mip_offsets = { 0 };

	auto pixels = std::vector <float4_32b> (loaded->dataf.size() / 4);
	for (size_t i = 0, o = 0; i + 3 < loaded->dataf.size(); i += 4, ++o) {
		pixels[o] = float4_32b(
			loaded->dataf[i + 0],
			loaded->dataf[i + 1],
			loaded->dataf[i + 2],
			loaded->dataf[i + 3]
		);
	}

	assign_pixels(tex, std::span <const float4_32b> (pixels));
	return tex;
}

} // namespace mrd::impl
