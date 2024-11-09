// SPDX-License-Identifier: Zlib OR GPL-3.0-or-later
//
// Copyright (c) 2023 Adrian "asie" Siekierka

#pragma once
#include <cstdlib>
#include <cstring>

#define RGB15(r, g, b) ((r) | (g<<5) | (b<<10))

// Maximum frame sequence count for banner icons.
#define RASTER_MAX_FRAME_COUNT 64

// The code supports up to 256 palette colors without modifications
// (unsigned char limit), but banners only support up to 16 colors.
#define RASTER_MAX_PALETTE_SIZE 16

// Check if a given filename has a RasterImage-compatible extension.
bool IsRasterImageExtensionFilename(const char *filename);
bool IsBmpExtensionFilename(const char *filename);

struct RasterRgbQuad
{
	unsigned char a, b, g, r;

	RasterRgbQuad() {}

	RasterRgbQuad(unsigned int pxl, bool rgb15) {
		if (rgb15) {
			a = ((pxl >> 15) & 0x01) * 255;
			b = ((pxl >> 10) & 0x1F) * 255 / 31;
			g = ((pxl >> 5) & 0x1F) * 255 / 31;
			r = (pxl & 0x1F) * 255 / 31;
		} else {
			a = pxl >> 24;
			b = pxl >> 16;
			g = pxl >> 8;
			r = pxl >> 0;
		}
	}

	inline unsigned short rgb15() {
		return RGB15(r >> 3, g >> 3, b >> 3);
	}

	inline unsigned int rgb24() {
		return r | (g << 8) | (b << 16) | (r << 24);
	}
};

inline bool operator==(const RasterRgbQuad& left, const RasterRgbQuad& right) {
	const unsigned int *left_i = (const unsigned int*) &left;
	const unsigned int *right_i = (const unsigned int*) &right;
	return *left_i == *right_i;
}

inline bool operator<(const RasterRgbQuad& left, const RasterRgbQuad& right) {
	const unsigned int *left_i = (const unsigned int*) &left;
	const unsigned int *right_i = (const unsigned int*) &right;
	return *left_i < *right_i;
}

inline bool operator!=(const RasterRgbQuad& left, const RasterRgbQuad& right) {
	return !(left == right);
}

struct RasterImage
{
	int width, height, frames, components;
	unsigned char *data; // Format depends on components (see get_pixel, stb_image.h)
	int *delays; // 1/1000 second units
	unsigned int palette_count[RASTER_MAX_FRAME_COUNT];
	unsigned int palette[RASTER_MAX_FRAME_COUNT][RASTER_MAX_PALETTE_SIZE]; // Format matches components
	bool has_palette;
	bool is_subimage;

	RasterImage() {
		width = 0;
		height = 0;
		frames = 1;
		components = 4;
		data = NULL;
		delays = NULL;
		has_palette = false;
		is_subimage = false;
	}

	RasterImage(int _width, int _height, int _frames = 1, int _components = 4) {
		width = _width;
		height = _height;
		frames = _frames;
		components = _components;
		has_palette = false;
		is_subimage = false;
		data = (unsigned char*) malloc(frames * width * height * get_component_size());
		delays = NULL;
	}

	~RasterImage();

	// Return the component size.
	inline unsigned int get_component_size(void) const {
		return has_palette ? 1 : components;
	}

	// Return the pixel data (color or palette entry) for a coordinate.
	unsigned int get_data(int z, int x, int y) const;

	// Set the pixel data (color or palette entry) for a coordinate.
	void set_data(int z, int x, int y, unsigned int v);

	// Return the RGB pixel color for a coordinate.
	inline RasterRgbQuad get_pixel(int z, int x, int y) const {
		RasterRgbQuad result;
		unsigned int pxl = get_data(z, x, y);
		if (has_palette) pxl = palette[z][pxl];
		switch (components) {
			case 2:
				result.a = pxl >> 8;
				// fall through
			case 1:
				result.r = result.g = result.b = pxl;
				break;
			case 4:
				result.a = pxl >> 24;
				// fall through
			case 3:
				result.b = pxl >> 16;
				result.g = pxl >> 8;
				result.r = pxl >> 0;
				break;
		}
		return result;
	}

	// Return a RasterImage representing only a specific frame.
	// It is not a copy - it depends on the existence of the previous struct.
	inline const RasterImage subimage(int z) const {
		RasterImage s = {};
		s.width = width;
		s.height = height;
		s.frames = 1;
		s.components = components;
		s.data = data + (z * width * height * get_component_size());
		s.delays = NULL;
		s.has_palette = has_palette;
		if (has_palette) {
			s.palette_count[0] = palette_count[z];
			memcpy(s.palette[0], palette[z], sizeof(int) * RASTER_MAX_PALETTE_SIZE);
		}
		s.is_subimage = true;
		return s;
	}

	// Returns a cloned RasterImage with the specific transformations applied.
	RasterImage *clone(bool flip_h = false, bool flip_v = false) const;

	// Return the maximum palette count across all frames.
	int max_palette_count(void) const;

	// Load an image file. BMP, GIF and PNG files are supported.
	// For animation, only GIF files are supported.
	bool loadFile(const char *filename);

	// Load an image file from memory.
	bool loadBuffer(const void *data, size_t length, const char *name);

	// Save an image file.
	bool saveFile(const char *filename);

	// Quantize the color-values to match the RGB15 format (1-bit alpha, 5-bit colors).
	bool quantize_rgb15(void);

	// Convert a non-paletted image to a paletted image.
	bool convert_palette(void);

	// Convert a paletted image such that index 0, and only index 0, is transparent.
	bool make_zero_transparent(void);

protected:
	bool loadStb(void *ctx, const char *filename);
};

// The == operator checks for pixel-level equivalency, not data-level equivalency.
inline bool operator==(const RasterImage& left, const RasterImage& right) {
	if (left.width != right.width) return false;
	if (left.height != right.height) return false;
	if (left.frames != right.frames) return false;
	if (left.components != right.components) return false;
	for (int z = 0; z < left.frames; z++) {
		for (int y = 0; y < left.height; y++) {
			for (int x = 0; x < left.width; x++) {
				if (left.get_pixel(z, x, y) != right.get_pixel(z, x, y)) return false;
			}
		}
	}
	return true;
}

inline bool operator!=(const RasterImage& left, const RasterImage& right) {
	return !(left == right);
}
