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
#include <inttypes.h>
#include "mali_gralloc_formats.h"
#include "format_info.h"

/* Default width aligned to whole pixel. */
#define PWA_DEFAULT .pwa = 1

/*
 * Format table, containing format properties.
 *
 * NOTE: This table should only be used within
 * the gralloc library and not by clients directly.
 */
const format_info_t formats[] = {

	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_565,         .npln = 1, .ncmp = 3, .bps = 6,  .bpp_afbc = { 16, 0, 0 },  .bpp = { 16, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = true,  .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_888,         .npln = 1, .ncmp = 3, .bps = 8,  .bpp_afbc = { 24, 0, 0 },  .bpp = { 24, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888,       .npln = 1, .ncmp = 4, .bps = 8,  .bpp_afbc = { 32, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = true,  .is_yuv = false, .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888,       .npln = 1, .ncmp = 4, .bps = 8,  .bpp_afbc = { 32, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = true,  .is_yuv = false, .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888,       .npln = 1, .ncmp = 3, .bps = 8,  .bpp_afbc = { 32, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
#if PLATFORM_SDK_VERSION >= 26
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102,    .npln = 1, .ncmp = 4, .bps = 10, .bpp_afbc = { 32, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = true,  .is_yuv = false, .afbc = true,  .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616,   .npln = 1, .ncmp = 4, .bps = 16, .bpp_afbc = { 64, 0, 0 },  .bpp = { 64, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = true,  .is_yuv = false, .afbc = false, .linear = true,  .flex = true,  PWA_DEFAULT },
#endif /* PLATFORM_SDK_VERSION >= 26 */

	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_Y8,              .npln = 1, .ncmp = 1, .bps = 8,  .bpp_afbc = { 8, 0, 0 },   .bpp = { 8, 0, 0 },   .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  .pwa = 16   },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_Y16,             .npln = 1, .ncmp = 1, .bps = 16, .bpp_afbc = { 16, 0, 0 },  .bpp = { 16, 0, 0 },  .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  .pwa = 16   },

	/* 420 (8-bit) */
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I,   .npln = 1, .ncmp = 3, .bps = 8,  .bpp_afbc = { 12, 0, 0 },  .bpp = { 0, 0, 0 },   .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = false, .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_NV12,            .npln = 2, .ncmp = 3, .bps = 8,  .bpp_afbc = { 8, 16, 0 },  .bpp = { 8, 16, 0 },  .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_NV21,            .npln = 2, .ncmp = 3, .bps = 8,  .bpp_afbc = { 8, 16, 0 },  .bpp = { 8, 16, 0 },  .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_YCrCb_420_SP,                .npln = 2, .ncmp = 3, .bps = 8,  .bpp_afbc = { 8, 16, 0 },  .bpp = { 8, 16, 0 },  .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_YV12,            .npln = 3, .ncmp = 3, .bps = 8,  .bpp_afbc = { 8, 8, 8 },   .bpp = { 8, 8, 8 },   .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  .pwa = 16   },

	/* 422 (8-bit) */
	{ .id = HAL_PIXEL_FORMAT_YCbCr_422_I,                 .npln = 1, .ncmp = 3, .bps = 8,  .bpp_afbc = { 16, 0, 0 },  .bpp = { 16, 0, 0 },  .hsub = 2, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_YCbCr_422_SP,                .npln = 2, .ncmp = 3, .bps = 8,  .bpp_afbc = { 8, 16, 0 },  .bpp = { 8, 16, 0 },  .hsub = 2, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },

	/* 420 (10-bit) */
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I,  .npln = 1, .ncmp = 3, .bps = 10, .bpp_afbc = { 15, 0, 0 },  .bpp = { 0, 0, 0 },   .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = false, .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_Y0L2,            .npln = 1, .ncmp = 4, .bps = 10, .bpp_afbc = { 16, 0, 0 },  .bpp = { 16, 0, 0 },  .hsub = 2, .vsub = 2, .tile_size = 2, .has_alpha = true,  .is_yuv = true,  .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_P010,            .npln = 2, .ncmp = 3, .bps = 10, .bpp_afbc = { 10, 20, 0 }, .bpp = { 16, 32, 0 }, .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },

	/* 422 (10-bit) */
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_Y210,            .npln = 1, .ncmp = 3, .bps = 10, .bpp_afbc = { 20, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 2, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_P210,            .npln = 2, .ncmp = 3, .bps = 10, .bpp_afbc = { 10, 20, 0 }, .bpp = { 16, 32, 0 }, .hsub = 2, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = true,  .flex = true,  PWA_DEFAULT },

	/* 444 (10-bit) */
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_YUV444_10BIT_I,  .npln = 1, .ncmp = 3, .bps = 10, .bpp_afbc = { 30, 0, 0 },  .bpp = { 0, 0, 0 },   .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = true,  .afbc = true,  .linear = false, .flex = false, PWA_DEFAULT },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_Y410,            .npln = 1, .ncmp = 4, .bps = 10, .bpp_afbc = { 32, 0, 0 },  .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = true,  .is_yuv = true,  .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },

	/* Other */
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RAW16,           .npln = 1, .ncmp = 1, .bps = 16, .bpp_afbc = { 16, 0, 0 },  .bpp = { 16, 0, 0 },  .hsub = 2, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, .pwa = 16   },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RAW12,           .npln = 1, .ncmp = 1, .bps = 12, .bpp_afbc = { 12, 0, 0 },  .bpp = { 12, 0, 0 },  .hsub = 4, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, .pwa = 4    },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_RAW10,           .npln = 1, .ncmp = 1, .bps = 10, .bpp_afbc = { 10, 0, 0 },  .bpp = { 10, 0, 0 },  .hsub = 4, .vsub = 2, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, .pwa = 4    },
	{ .id = MALI_GRALLOC_FORMAT_INTERNAL_BLOB,            .npln = 1, .ncmp = 1, .bps = 8,  .bpp_afbc = { 8, 0, 0 },   .bpp = { 8, 0, 0 },   .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },

#if PLATFORM_SDK_VERSION >= 28
	/* Depth and Stencil */
	{ .id = HAL_PIXEL_FORMAT_DEPTH_16,                    .npln = 1, .ncmp = 1, .bps = 16, .bpp_afbc = { 0, 0, 0},    .bpp = { 16, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_DEPTH_24,                    .npln = 1, .ncmp = 1, .bps = 24, .bpp_afbc = { 0, 0, 0 },   .bpp = { 24, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_DEPTH_24_STENCIL_8,          .npln = 1, .ncmp = 2, .bps = 24, .bpp_afbc = { 0, 0, 0 },   .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_DEPTH_32F,                   .npln = 1, .ncmp = 1, .bps = 32, .bpp_afbc = { 0, 0, 0 },   .bpp = { 32, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_DEPTH_32F_STENCIL_8,         .npln = 1, .ncmp = 2, .bps = 32, .bpp_afbc = { 0, 0, 0 },   .bpp = { 40, 0, 0 },  .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
	{ .id = HAL_PIXEL_FORMAT_STENCIL_8,                   .npln = 1, .ncmp = 1, .bps = 8,  .bpp_afbc = { 0, 0, 0 },   .bpp = { 8, 0, 0 },   .hsub = 1, .vsub = 1, .tile_size = 1, .has_alpha = false, .is_yuv = false, .afbc = false, .linear = true,  .flex = false, PWA_DEFAULT },
#endif
};

const size_t num_formats = sizeof(formats)/sizeof(formats[0]);

/*
 *  Finds "Look-up Table" index for the given format
 *
 * @param base_format [in]   Format for which index is required.
 *
 * @return index, when the format is found in the look up table
 *         -1, otherwise
 *
 */
int32_t get_format_index(const uint64_t base_format)
{
	uint32_t format_idx;
	for (format_idx = 0; format_idx < num_formats; format_idx++)
	{
		if (formats[format_idx].id == (uint32_t)base_format)
		{
			break;
		}
	}
	if (format_idx >= num_formats)
	{
		return -1;
	}

	return (int32_t)format_idx;
}
