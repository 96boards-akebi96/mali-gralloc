/*
 * Copyright (C) 2018 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FORMAT_INFO_H_
#define FORMAT_INFO_H_

#include "mali_gralloc_buffer.h"

typedef struct
{
	uint16_t width;
	uint16_t height;
} rect_t;


/*
 * Pixel format information.
 *
 * These properties are used by gralloc for buffer allocation.
 * Each format is uniquely identified with 'id'.
 */
typedef struct
{
	uint32_t id;                    /* Format ID. */
	uint8_t npln;                   /* Number of planes. */
	uint8_t ncmp;                   /* Number of components. */
	uint8_t bps;                    /* Bits per sample (primary/largest). */
	uint8_t bpp_afbc[MAX_PLANES];   /* Bits per pixel (AFBC), without implicit padding. 'X' in RGBX is still included. */
	uint8_t bpp[MAX_PLANES];        /* Bits per pixel (linear/uncompressed), including any implicit sample padding defined by format (e.g. 10-bit Y210 padded to 16-bits).
	                                 * NOTE: bpp[n] and/or (bpp[n] * pwa) must be multiples of 8. */
	uint8_t hsub;                   /* Horizontal sub-sampling (YUV formats). Pixel rounding in width (all formats). */
	uint8_t vsub;                   /* Vertical sub-sampling (YUV formats). Pixel rounding in height (all formats). */
	uint16_t tile_size;             /* Tile size (in pixels), assumed square. Uncompressed only. */
	bool has_alpha;                 /* Alpha channel present. */
	bool is_yuv;                    /* YUV format (contains luma *and* chroma). */
	bool afbc;                      /* AFBC supported (per specification and by gralloc). IP support not considered. */
	bool linear;                    /* Linear/uncompressed supported. */
	bool flex;                      /* Linear version of format can be represented as flex. */
	uint8_t pwa;                    /* Width alignment for each plane (in pixels). PWA_DEFAULT: 1. Must be a power of 2. */

} format_info_t;


extern const format_info_t formats[];
extern const size_t num_formats;
extern int32_t get_format_index(const uint64_t base_format);

#endif
