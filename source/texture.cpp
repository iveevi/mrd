#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT 0
#include <stb_image.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "texture.hpp"

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

	LoadedImage img;

	const auto path_str = path.string();
	int w = 0, h = 0, channels = 0;

	img.hdr = stbi_is_hdr(path_str.c_str()) == 1;

	if (img.hdr) {
		float *pixels = stbi_loadf(path_str.c_str(), &w, &h, &channels, 4);
		if (!pixels)
			return std::unexpected(TextureError(TextureError::Code::StbFailure, stbi_failure_reason()));

		img.dataf.assign(pixels, pixels + static_cast <size_t> (w) * h * 4);
		stbi_image_free(pixels);
	} else {
		unsigned char *pixels = stbi_load(path_str.c_str(), &w, &h, &channels, 4);
		if (!pixels)
			return std::unexpected(TextureError(TextureError::Code::StbFailure, stbi_failure_reason()));

		img.data8.assign(pixels, pixels + static_cast <size_t> (w) * h * 4);
		stbi_image_free(pixels);
	}

	img.width = w;
	img.height = h;
	img.channels = 4;

	if (img.width == 0 || img.height == 0 || (img.data8.empty() && img.dataf.empty()))
		return std::unexpected(TextureError(TextureError::Code::EmptyImage, "empty image or load failure"));

	if (img.width > options.max_size || img.height > options.max_size) {
		return std::unexpected(TextureError(
			TextureError::Code::SizeOutOfRange,
			"image dimensions exceed TextureLoadOptions::max_size"
		));
	}

	if (options.generate_mips) {
		return std::unexpected(TextureError(
			TextureError::Code::UnsupportedEncoding,
			"mip generation is not implemented yet"
		));
	}

	return img;
}

std::expected <CanonicalLDR, TextureError> load_canonical_ldr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
)
{
	auto loaded = load_with_stb(path, options);
	if (!loaded)
		return std::unexpected(loaded.error());

	if (loaded->hdr) {
		return std::unexpected(TextureError(
			TextureError::Code::UnsupportedEncoding,
			"requested LDR loader but image is HDR"
		));
	}

	CanonicalLDR tex;
	tex.desc.size = { loaded->width, loaded->height };
	tex.desc.is_srgb = true;
	tex.desc.mipmapped = false;
	tex.mip_offsets = { 0 };
	tex.data.resize(loaded->data8.size() / 4);

	for (size_t i = 0, o = 0; i + 3 < loaded->data8.size(); i += 4, ++o) {
		tex.data[o] = {
			loaded->data8[i + 0],
			loaded->data8[i + 1],
			loaded->data8[i + 2],
			loaded->data8[i + 3],
		};
	}

	return tex;
}

std::expected <CanonicalHDR, TextureError> load_canonical_hdr(
	const std::filesystem::path &path,
	const TextureLoadOptions &options
)
{
	auto loaded = load_with_stb(path, options);
	if (!loaded)
		return std::unexpected(loaded.error());

	if (!loaded->hdr) {
		return std::unexpected(TextureError(
			TextureError::Code::UnsupportedEncoding,
			"requested HDR loader but image is LDR"
		));
	}

	CanonicalHDR tex;
	tex.desc.size = { loaded->width, loaded->height };
	tex.desc.is_srgb = options.force_srgb;
	tex.desc.mipmapped = false;
	tex.mip_offsets = { 0 };
	tex.data.resize(loaded->dataf.size() / 4);

	for (size_t i = 0, o = 0; i + 3 < loaded->dataf.size(); i += 4, ++o) {
		tex.data[o] = {
			loaded->dataf[i + 0],
			loaded->dataf[i + 1],
			loaded->dataf[i + 2],
			loaded->dataf[i + 3],
		};
	}

	return tex;
}

} // namespace mrd::impl
