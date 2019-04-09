/*
 * Copyright (C) 2016-2018 ARM Limited. All rights reserved.
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

#include <hardware/hardware.h>
#include <inttypes.h>
#include <assert.h>
#include <atomic>

#if GRALLOC_USE_GRALLOC1_API == 1
#include <hardware/gralloc1.h>
#else
#include <hardware/gralloc.h>
#endif

#include "mali_gralloc_module.h"
#include "mali_gralloc_bufferallocation.h"
#include "mali_gralloc_ion.h"
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_buffer_priv.h"
#include "mali_gralloc_bufferdescriptor.h"
#include "mali_gralloc_debug.h"
#include "format_info.h"

#if GRALLOC_USE_LEGACY_CALCS == 1
#include "legacy/buffer_alloc.h"
#endif

#define AFBC_PIXELS_PER_BLOCK 16
#define AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY 16

static int mali_gralloc_buffer_free_internal(buffer_handle_t *pHandle, uint32_t num_hnds);
bool afbc_format_fallback(uint32_t * const format_idx, const uint64_t usage, bool force);

/*
 * AFBC superblock type.
*/
enum AllocBaseType
{
	UNCOMPRESSED,       /* No compression. */
	AFBC,               /* AFBC basic (16x16). */
	AFBC_WIDEBLK,       /* AFBC wide-block (32x8). */
	AFBC_EXTRAWIDEBLK,  /* AFBC extra-wide-block (64x4). */
};

/*
 * Allocation type.
 *
 * Allocation-specific properties of the AFBC format modifiers
 * described by MALI_GRALLOC_INTFMT_*.
 *
 */
typedef struct {

	/*
	 * AFBC superblock type for either:
	 * - single plane AFBC format or
	 * - first/luma plane of multi-plane AFBC format.
	 */
	AllocBaseType primary_type;

	/*
	 * Multi-plane AFBC format. AFBC chroma-only plane(s) are
	 * always compressed with superblock type 'AFBC_EXTRAWIDEBLK'.
	 */
	bool is_multi_plane;

	/*
	 * Allocate tiled AFBC headers.
	 */
	bool is_tiled;

	/*
	 * Pad AFBC header stride to 64-byte alignment
	 * (multiple of 4x16B headers).
	 */
	bool is_padded;

	/*
	 * Front-buffer rendering safe AFBC allocations include an
	 * additional 4kB-aligned body buffer.
	 */
	bool is_frontbuffer_safe;

	bool is_afbc() const
	{
		return primary_type != UNCOMPRESSED;
	}
} alloc_type_t;


/*
 * Get a global unique ID
 */
static uint64_t getUniqueId()
{
	static std::atomic<uint32_t> counter(0);
	uint64_t id = static_cast<uint64_t>(getpid()) << 32;
	return id | counter++;
}

static void afbc_buffer_align(const bool is_tiled, int *size)
{
	const uint16_t AFBC_BODY_BUFFER_BYTE_ALIGNMENT = 1024;

	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	if (is_tiled)
	{
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}

	*size = GRALLOC_ALIGN(*size, buffer_byte_alignment);
}

/*
 * Obtain AFBC superblock dimensions from type.
 */
static rect_t get_afbc_sb_size(AllocBaseType alloc_base_type)
{
	const uint16_t AFBC_BASIC_BLOCK_WIDTH = 16;
	const uint16_t AFBC_BASIC_BLOCK_HEIGHT = 16;
	const uint16_t AFBC_WIDE_BLOCK_WIDTH = 32;
	const uint16_t AFBC_WIDE_BLOCK_HEIGHT = 8;
	const uint16_t AFBC_EXTRAWIDE_BLOCK_WIDTH = 64;
	const uint16_t AFBC_EXTRAWIDE_BLOCK_HEIGHT = 4;

	rect_t sb = {0, 0};

	switch(alloc_base_type)
	{
		case UNCOMPRESSED:
			break;
		case AFBC:
			sb.width = AFBC_BASIC_BLOCK_WIDTH;
			sb.height = AFBC_BASIC_BLOCK_HEIGHT;
			break;
		case AFBC_WIDEBLK:
			sb.width = AFBC_WIDE_BLOCK_WIDTH;
			sb.height = AFBC_WIDE_BLOCK_HEIGHT;
			break;
		case AFBC_EXTRAWIDEBLK:
			sb.width = AFBC_EXTRAWIDE_BLOCK_WIDTH;
			sb.height = AFBC_EXTRAWIDE_BLOCK_HEIGHT;
			break;
	}
	return sb;
}

/*
 * Obtain AFBC superblock dimensions for specific plane.
 *
 * See alloc_type_t for more information.
 */
static rect_t get_afbc_sb_size(alloc_type_t alloc_type, const uint8_t plane)
{
	if (plane > 0 && alloc_type.is_afbc() && alloc_type.is_multi_plane)
	{
		return get_afbc_sb_size(AFBC_EXTRAWIDEBLK);
	}
	else
	{
		return get_afbc_sb_size(alloc_type.primary_type);
	}
}


static bool get_alloc_type(const uint64_t internal_format,
                           const uint32_t format_idx,
                           const uint64_t usage,
                           alloc_type_t * const alloc_type)
{
	alloc_type->primary_type = UNCOMPRESSED;
	alloc_type->is_multi_plane = formats[format_idx].npln > 1;
	alloc_type->is_tiled = false;
	alloc_type->is_padded = false;
	alloc_type->is_frontbuffer_safe = false;

	/* Determine AFBC type for this format. This is used to decide alignment.
	   Split block does not affect alignment, and therefore doesn't affect the allocation type. */
	if (internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK)
	{
		/* Determine primary AFBC (superblock) type. */
		alloc_type->primary_type = AFBC;
		if (internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
		{
			alloc_type->primary_type = AFBC_WIDEBLK;
		}
		else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)
		{
			alloc_type->primary_type = AFBC_EXTRAWIDEBLK;
		}

		if (internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
		{
			alloc_type->is_tiled = true;

			if (formats[format_idx].npln > 1 &&
				(internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK) == 0)
			{
				ALOGW("Extra-wide AFBC must be signalled for multi-plane formats. "
				      "Falling back to single plane AFBC.");
				alloc_type->is_multi_plane = false;
			}

			if (internal_format & MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)
			{
				alloc_type->is_frontbuffer_safe = true;
			}
		}
		else
		{
			if (formats[format_idx].npln > 1)
			{
				ALOGW("Multi-plane AFBC is not supported without tiling. "
				      "Falling back to single plane AFBC.");
			}
			alloc_type->is_multi_plane = false;
		}

		if (internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK &&
			!alloc_type->is_tiled)
		{
			/* Headers must be tiled for extra-wide. */
			ALOGE("ERROR: Invalid to specify extra-wide block without tiled headers.");
			return false;
		}

		if (alloc_type->is_frontbuffer_safe &&
		    (internal_format & (MALI_GRALLOC_INTFMT_AFBC_WIDEBLK | MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)))
		{
			ALOGE("ERROR: Front-buffer safe not supported with wide/extra-wide block.");
		}

		if (formats[format_idx].npln == 1 &&
		    internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK &&
		    internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)
		{
			/* "Wide + Extra-wide" implicitly means "multi-plane". */
			ALOGE("ERROR: Invalid to specify multiplane AFBC with single plane format.");
			return false;
		}

		if (usage & MALI_GRALLOC_USAGE_AFBC_PADDING)
		{
			alloc_type->is_padded = true;
		}
	}

	return true;
}

/*
 * Initialise AFBC header based on superblock layout.
 * Width and height should already be AFBC aligned.
 */
void init_afbc(uint8_t *buf, const uint64_t internal_format,
               const bool is_multi_plane,
               const int w, const int h)
{
	const bool is_tiled = ((internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
	                         == MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS);
	const uint32_t n_headers = w * h / (AFBC_PIXELS_PER_BLOCK * AFBC_PIXELS_PER_BLOCK);
	int body_offset = n_headers * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;

	afbc_buffer_align(is_tiled, &body_offset);

	/*
	 * Declare the AFBC header initialisation values for each superblock layout.
	 * Tiled headers (AFBC 1.2) can be initialised to zero for non-subsampled formats
	 * (SB layouts: 0, 3, 4, 7).
	 */
	uint32_t headers[][4] = {
		{ (uint32_t)body_offset, 0x1, 0x10000, 0x0 }, /* Layouts 0, 3, 4, 7 */
		{ ((uint32_t)body_offset + (1 << 28)), 0x80200040, 0x1004000, 0x20080 } /* Layouts 1, 5 */
	};
	if ((internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS))
	{
		/* Zero out body_offset for non-subsampled formats. */
		memset(headers[0], 0, sizeof(uint32_t) * 4);
	}

	/* Map base format to AFBC header layout */
	const uint64_t base_format = internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

	/* Sub-sampled formats use layouts 1 and 5 which is index 1 in the headers array.
	 * 1 = 4:2:0 16x16, 5 = 4:2:0 32x8.
	 *
	 * Non-subsampled use layouts 0, 3, 4 and 7, which is index 0.
	 * 0 = 16x16, 3 = 32x8 + split, 4 = 32x8, 7 = 64x4.
	 *
	 * When using separated planes for YUV formats, the header layout is the non-subsampled one
	 * as there is a header per-plane and there is no sub-sampling within the plane.
	 * Separated plane only supports 32x8 or 64x4 for the luma plane, so the first plane must be 4 or 7.
	 * Seperated plane only supports 64x4 for subsequent planes, so these must be header layout 7.
	 */
	const uint32_t layout = is_subsampled_yuv(base_format) && !is_multi_plane ? 1 : 0;

	ALOGV("Writing AFBC header layout %d for format %" PRIu64, layout, base_format);

	for (uint32_t i = 0; i < n_headers; i++)
	{
		memcpy(buf, headers[layout], sizeof(headers[layout]));
		buf += sizeof(headers[layout]);
	}
}

static int max(int a, int b)
{
	return a > b ? a : b;
}

static int max(int a, int b, int c)
{
	return c > max(a, b) ? c : max(a, b);
}

static int max(int a, int b, int c, int d)
{
	return d > max(a, b, c) ? d : max(a, b, c);
}

/*
 * Obtain plane allocation dimensions (in pixels).
 *
 * NOTE: pixel stride, where defined for format, is
 * incorporated into allocation dimensions.
 */
static void get_pixel_w_h(uint32_t * const width,
                          uint32_t * const height,
                          const format_info_t format,
                          const alloc_type_t alloc_type,
                          const uint8_t plane,
                          bool has_cpu_usage)
{
	const rect_t sb = get_afbc_sb_size(alloc_type, plane);

	/*
	 * Round-up plane dimensions, to multiple of:
	 * - Samples for all channels (sub-sampled formats)
	 * - Memory bytes/words (some packed formats)
	 */
	*width = GRALLOC_ALIGN(*width, format.hsub);
	*height = GRALLOC_ALIGN(*height, format.vsub);

	/*
	 * Sub-sample (sub-sampled) planes.
	 */
	if (plane > 0)
	{
		*width /= format.hsub;
		*height /= format.vsub;
	}

	/*
	 * Pixel alignment (width),
	 * where format stride is stated in pixels.
	 */
	int pixel_align_w = 0;
	if (has_cpu_usage)
	{
		pixel_align_w = format.pwa;
	}
	else if (alloc_type.is_afbc())
	{
#define HEADER_STRIDE_ALIGN_IN_SUPER_BLOCKS (0)
		uint32_t num_sb_align = 0;
		if (alloc_type.is_padded && !format.is_yuv)
		{
			/* Align to 4 superblocks in width --> 64-byte,
			 * assuming 16-byte header per superblock.
			 */
			num_sb_align = 4;
		}
		pixel_align_w = max(HEADER_STRIDE_ALIGN_IN_SUPER_BLOCKS, num_sb_align) * sb.width;
	}

	/*
	 * Determine AFBC tile size when allocating tiled headers.
	 */
	rect_t afbc_tile = sb;
	if (alloc_type.is_tiled)
	{
		afbc_tile.width = format.bpp_afbc[plane] > 32 ? 4 * afbc_tile.width : 8 * afbc_tile.width;
		afbc_tile.height = format.bpp_afbc[plane] > 32 ? 4 * afbc_tile.height : 8 * afbc_tile.height;
	}

	ALOGV("Plane[%hhu]: [SUB-SAMPLE] w:%d, h:%d\n", plane, *width, *height);
	ALOGV("Plane[%hhu]: [PIXEL_ALIGN] w:%d\n", plane, pixel_align_w);
	ALOGV("Plane[%hhu]: [LINEAR_TILE] w:%" PRIu16 "\n", plane, format.tile_size);
	ALOGV("Plane[%hhu]: [AFBC_TILE] w:%" PRIu16 ", h:%" PRIu16 "\n", plane, afbc_tile.width, afbc_tile.height);

	*width = GRALLOC_ALIGN(*width, max(1, pixel_align_w, format.tile_size, afbc_tile.width));
	*height = GRALLOC_ALIGN(*height, max(1, format.tile_size, afbc_tile.height));
}



static uint32_t gcd(uint32_t a, uint32_t b)
{
	uint32_t r, t;

	if (a == b)
	{
		return a;
	}
	else if (a < b)
	{
		t = a;
		a = b;
		b = t;
	}

	while (b != 0)
	{
		r = a % b;
		a = b;
		b = r;
	}

	return a;
}

uint32_t lcm(uint32_t a, uint32_t b)
{
	if (a != 0 && b != 0)
	{
		return (a * b) / gcd(a, b);
	}

	return max(a, b);
}


/*
 * YV12 stride has additional complexity since chroma stride
 * must conform to the following:
 *
 * c_stride = ALIGN(stride/2, 16)
 *
 * Since the stride alignment must satisfy both CPU and HW
 * constraints, the luma stride must be doubled.
 */
static void update_yv12_stride(int8_t plane,
                               uint32_t luma_stride,
                               uint32_t stride_align,
                               uint32_t * byte_stride)
{
	if (plane == 0)
	{
		/*
		 * Ensure luma stride is aligned to "2*lcm(hw_align, cpu_align)" so
		 * that chroma stride can satisfy both CPU and HW alignment
		 * constraints when only half luma stride (as mandated for format).
		 */
		*byte_stride = GRALLOC_ALIGN(luma_stride, 2 * stride_align);
	}
	else
	{
		/*
		 * Derive chroma stride from luma and verify it is:
		 * 1. Aligned to lcm(hw_align, cpu_align)
		 * 2. Multiple of 16px (16 bytes)
		 */
		*byte_stride = luma_stride / 2;
		assert(*byte_stride == GRALLOC_ALIGN(*byte_stride, stride_align));
		assert(*byte_stride & 15 == 0);
	}
}



/*
 * Calculate allocation size.
 *
 * Determine the width and height of each plane based on pixel alignment for
 * both uncompressed and AFBC allocations.
 *
 * @param width           [in]    Buffer width.
 * @param height          [in]    Buffer height.
 * @param alloc_type      [in]    Allocation type inc. whether tiled and/or multi-plane.
 * @param format          [in]    Pixel format.
 * @param has_cpu_usage   [in]    CPU usage requested (in addition to any other).
 * @param pixel_stride    [out]   Calculated pixel stride.
 * @param size            [out]   Total calculated buffer size including all planes.
 * @param plane_info      [out]   Array of calculated information for each plane. Includes
 *                                offset, byte stride and allocation width and height.
 */
static void calc_allocation_size(const int width,
                                 const int height,
                                 const alloc_type_t alloc_type,
                                 const format_info_t format,
                                 const bool has_cpu_usage,
                                 const bool has_hw_usage,
                                 int * const pixel_stride,
                                 size_t * const size,
                                 plane_info_t plane_info[MAX_PLANES])
{
	plane_info[0].offset = 0;

	*size = 0;
	for (uint8_t plane = 0; plane < format.npln; plane++)
	{
		plane_info[plane].alloc_width = width;
		plane_info[plane].alloc_height = height;
		get_pixel_w_h(&plane_info[plane].alloc_width,
		              &plane_info[plane].alloc_height,
		              format,
		              alloc_type,
		              plane,
		              has_cpu_usage);
		ALOGV("Aligned w=%d, h=%d (in pixels)",
		      plane_info[plane].alloc_width, plane_info[plane].alloc_height);

		/*
		 * Calculate byte stride (per plane).
		 */
		if (alloc_type.is_afbc())
		{
			assert((plane_info[plane].alloc_width * format.bpp_afbc[plane]) % 8 == 0);
			plane_info[plane].byte_stride = (plane_info[plane].alloc_width * format.bpp_afbc[plane]) / 8;
		}
		else
		{
			assert((plane_info[plane].alloc_width * format.bpp[plane]) % 8 == 0);
			plane_info[plane].byte_stride = (plane_info[plane].alloc_width * format.bpp[plane]) / 8;

			/*
			 * Align byte stride (uncompressed allocations only).
			 *
			 * Find the lowest-common-multiple of:
			 * 1. hw_align: Minimum byte stride alignment for HW IP (has_hw_usage == true)
			 * 2. cpu_align: Byte equivalent of 'pwa' (has_cpu_usage == true)
			 *
			 * NOTE: Pixel stride is defined as multiple of 'pwa'.
			 */
			uint16_t hw_align = 0;
			if (has_hw_usage)
			{
				hw_align = format.is_yuv ? 128 : 64;
			}

			uint32_t cpu_align = 0;
			if (has_cpu_usage)
			{
				assert((format.bpp[plane] * format.pwa) % 8 == 0);
				cpu_align = (format.bpp[plane] * format.pwa) / 8;
			}

			uint32_t stride_align = lcm(hw_align, cpu_align);
			plane_info[plane].byte_stride = GRALLOC_ALIGN(plane_info[plane].byte_stride, stride_align);

			/*
			 * Update YV12 stride with both CPU & HW usage due to constraint of chroma stride.
			 * Width is anyway aligned to 16px for luma and chroma (has_cpu_usage).
			 */
			if (format.id == MALI_GRALLOC_FORMAT_INTERNAL_YV12 && has_hw_usage && has_cpu_usage)
			{
				update_yv12_stride(plane,
				                   plane_info[0].byte_stride,
				                   stride_align,
				                   &plane_info[plane].byte_stride);
			}
		}
		ALOGV("Byte stride: %d", plane_info[plane].byte_stride);

		/*
		 * Pixel stride (CPU usage only).
		 * Not used in size calculation but exposed to client.
		 */
		if (plane == 0)
		{
			*pixel_stride = 0;

			if (!alloc_type.is_afbc() && has_cpu_usage)
			{
				assert((plane_info[plane].byte_stride * 8) % format.bpp[plane] == 0);
				*pixel_stride = (plane_info[plane].byte_stride * 8) / format.bpp[plane];
			}

			ALOGV("Pixel stride: %d", *pixel_stride);
		}

		const uint32_t sb_num = (plane_info[plane].alloc_width * plane_info[plane].alloc_height)
		                      / (AFBC_PIXELS_PER_BLOCK * AFBC_PIXELS_PER_BLOCK);

		/*
		 * Calculate body size (per plane).
		 */
		int body_size = 0;
		if (alloc_type.is_afbc())
		{
			const rect_t sb = get_afbc_sb_size(alloc_type, plane);
			const int sb_bytes = GRALLOC_ALIGN((format.bpp_afbc[plane] * sb.width * sb.height) / 8, 128);
			body_size = sb_num * sb_bytes;

			/* When AFBC planes are stored in separate buffers and this is not the last plane,
			   also align the body buffer to make the subsequent header aligned. */
			if (format.npln > 1 && plane < 2)
			{
				afbc_buffer_align(alloc_type.is_tiled, &body_size);
			}

			if (alloc_type.is_frontbuffer_safe)
			{
				int back_buffer_size = body_size;
				afbc_buffer_align(alloc_type.is_tiled, &back_buffer_size);
				body_size += back_buffer_size;
			}
		}
		else
		{
			body_size = (plane_info[plane].byte_stride) * plane_info[plane].alloc_height;
		}
		ALOGV("Body size: %d", body_size);


		/*
		 * Calculate header size (per plane).
		 */
		int header_size = 0;
		if (alloc_type.is_afbc())
		{
			/* As this is AFBC, calculate header size for this plane.
			 * Always align the header, which will make the body buffer aligned.
			 */
			header_size = sb_num * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
			afbc_buffer_align(alloc_type.is_tiled, &header_size);
		}
		ALOGV("AFBC Header size: %d", header_size);

		/*
		 * Set offset for separate chroma planes.
		 */
		if (plane > 0)
		{
			plane_info[plane].offset = *size;
		}

		/*
		 * Set overall size.
		 * Size must be updated after offset.
		 */
		*size += body_size + header_size;
		ALOGV("size=%zu",*size);
	}
}



/*
 * Validate selected format against requested.
 * Return true if valid, false otherwise.
 */
static bool validate_format(const format_info_t * const format,
                            const alloc_type_t alloc_type,
                            const buffer_descriptor_t * const bufDescriptor)
{
	if (alloc_type.is_afbc())
	{
		/*
		 * Validate format is supported by AFBC specification and gralloc.
		 */
		if (format->afbc == false)
		{
			ALOGE("ERROR: AFBC selected but not supported for base format: %" PRIx32, format->id);
			return false;
		}

		/*
		 * Enforce consistency between number of format planes and
		 * request for single/multi-plane AFBC.
		 */
		if (((format->npln == 1 && alloc_type.is_multi_plane) ||
		    (format->npln > 1 && !alloc_type.is_multi_plane)))
		{
			ALOGE("ERROR: Format (%" PRIx32 ", num planes: %u) is incompatible with %s-plane AFBC request",
			      format->id, format->npln, (alloc_type.is_multi_plane) ? "multi" : "single");
			return false;
		}
	}
	else
	{
		if (format->linear == false)
		{
			ALOGE("ERROR: Uncompressed format requested but not supported for base format: %" PRIx32, format->id);
			return false;
		}
	}

	if (format->id == MALI_GRALLOC_FORMAT_INTERNAL_BLOB &&
	    bufDescriptor->height != 1)
	{
		ALOGE("ERROR: Height for format BLOB must be 1.");
		return false;
	}

	return true;
}


int mali_gralloc_buffer_allocate(mali_gralloc_module *m, const gralloc_buffer_descriptor_t *descriptors,
                                 uint32_t numDescriptors, buffer_handle_t *pHandle, bool *shared_backend)
{
	bool shared = false;
	uint64_t backing_store_id = 0x0;
	alloc_type_t alloc_type;
	int err;
	static bool warn_about_mutual_exclusive = true;

	for (uint32_t i = 0; i < numDescriptors; i++)
	{
		buffer_descriptor_t * const bufDescriptor = (buffer_descriptor_t *)(descriptors[i]);
		int alloc_width = bufDescriptor->width;
		int alloc_height = bufDescriptor->height;
		uint64_t usage = bufDescriptor->producer_usage | bufDescriptor->consumer_usage;

		/*
		 * Select optimal internal pixel format based upon
		 * usage and requested format.
		 */
		bufDescriptor->internal_format = mali_gralloc_select_format(bufDescriptor->hal_format,
		                                                            bufDescriptor->format_type,
		                                                            usage,
		                                                            bufDescriptor->width * bufDescriptor->height);
		if (bufDescriptor->internal_format == 0)
		{
			ALOGE("ERROR: Unrecognized and/or unsupported format 0x%" PRIx64 " and usage 0x%" PRIx64,
			      bufDescriptor->hal_format, usage);
			return -EINVAL;
		}
		else if (warn_about_mutual_exclusive &&
		         (bufDescriptor->internal_format & 0x0000000100000000ULL) &&
		         (bufDescriptor->internal_format & 0x0000000e00000000ULL))
		{
			/*
			 * Modifier bits are no longer mutually exclusive. Warn when
			 * any bits are set in addition to AFBC basic since these might
			 * have been handled differently by clients under the old scheme.
			 * AFBC basic is guaranteed to be signalled when any other AFBC
			 * flags are set.
			 * This flag is to avoid the mutually exclusive modifier bits warning
			 * being continuously emitted. (see comment below for explanation of warning).
			 */
			warn_about_mutual_exclusive = false;
			ALOGW("WARNING: internal format modifier bits not mutually exclusive. "
			      "AFBC basic bit is always set, so extended AFBC support bits must always be checked.");
		}


		uint32_t format_idx;
		for (format_idx = 0; format_idx < num_formats; format_idx++)
		{
			if (formats[format_idx].id == (bufDescriptor->internal_format & MALI_GRALLOC_INTFMT_FMT_MASK))
			{
				break;
			}
		}
		if (format_idx >= num_formats)
		{
			ALOGE("ERROR: Allocation properties not found for selected format: %" PRIx64,
			      bufDescriptor->internal_format);
			return -EINVAL;
		}
		ALOGV("internal_format: %" PRIx64 " format_idx: %d", bufDescriptor->internal_format, format_idx);

		/*
		 * Obtain allocation type (uncompressed, AFBC basic, etc...)
		 */
		if (!get_alloc_type(bufDescriptor->internal_format, format_idx, usage, &alloc_type))
		{
			return -EINVAL;
		}

		if (alloc_type.primary_type != UNCOMPRESSED)
		{
			if (!afbc_format_fallback(&format_idx, usage, !alloc_type.is_multi_plane))
			{
				return -EINVAL;
			}
		}

		/* Store allocated format, which might be different from requested (due to fallback, etc.). */
		bufDescriptor->alloc_format = bufDescriptor->internal_format & MALI_GRALLOC_INTFMT_EXT_MASK;
		bufDescriptor->alloc_format |= formats[format_idx].id;

		/* Update multi-plane flag to indicate fall-back to single plane. */
		if (formats[format_idx].npln == 1)
		{
			alloc_type.is_multi_plane = false;
		}

		if (!validate_format(&formats[format_idx], alloc_type, bufDescriptor))
		{
			return -EINVAL;
		}

		/*
		 * Resolution of frame (allocation width and height) might require adjustment.
		 * This adjustment is only based upon specific usage and pixel format.
		 * If using AFBC, further adjustments to the allocation width and height will be made later
		 * based on AFBC alignment requirements and, for YUV, the plane properties.
		 */
		mali_gralloc_adjust_dimensions(bufDescriptor->internal_format,
		                               usage,
		                               &alloc_width,
		                               &alloc_height);

		/*
		* Obtain buffer size and plane information.
		*/
		calc_allocation_size(alloc_width,
		                     alloc_height,
		                     alloc_type,
		                     formats[format_idx],
		                     usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                     usage & ~(GRALLOC_USAGE_PRIVATE_MASK | GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                     &bufDescriptor->pixel_stride,
		                     &bufDescriptor->size,
		                     bufDescriptor->plane_info);

		bufDescriptor->old_byte_stride = bufDescriptor->plane_info[0].byte_stride;
		bufDescriptor->old_alloc_width = bufDescriptor->plane_info[0].alloc_width;
		bufDescriptor->old_alloc_height = bufDescriptor->plane_info[0].alloc_height;



#if GRALLOC_USE_LEGACY_CALCS == 1

		/* Translate to legacy alloc_type. */
		legacy::alloc_type_t legacy_alloc_type;
		switch (alloc_type.primary_type)
		{
			case AllocBaseType::AFBC:
				legacy_alloc_type.primary_type = legacy::AllocBaseType::AFBC;
				break;
			case AllocBaseType::AFBC_WIDEBLK:
				legacy_alloc_type.primary_type = legacy::AllocBaseType::AFBC_WIDEBLK;
				break;
			case AllocBaseType::AFBC_EXTRAWIDEBLK:
				legacy_alloc_type.primary_type = legacy::AllocBaseType::AFBC_EXTRAWIDEBLK;
				break;
			default:
				legacy_alloc_type.primary_type = legacy::AllocBaseType::UNCOMPRESSED;
				break;
		}
		if (alloc_type.is_padded)
		{
			legacy_alloc_type.primary_type = legacy::AllocBaseType::AFBC_PADDED;
		}
		legacy_alloc_type.is_multi_plane = alloc_type.is_multi_plane;
		legacy_alloc_type.is_tiled = alloc_type.is_tiled;


		/* Convert back to legacy YUV422_8BIT for size calculation. */
		uint64_t legacy_internal_format = bufDescriptor->internal_format;
		if (((legacy_internal_format & MALI_GRALLOC_INTFMT_FMT_MASK) == HAL_PIXEL_FORMAT_YCbCr_422_I) &&
		    ((bufDescriptor->hal_format & 0xffff) == MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT) &&
		    legacy_alloc_type.primary_type != legacy::AllocBaseType::UNCOMPRESSED)
		{
			legacy_internal_format &= ~MALI_GRALLOC_INTFMT_FMT_MASK;
			legacy_internal_format |= MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT;
		}

		/*
		 * Resolution of frame (and internal dimensions) might require adjustment
		 * based upon specific usage and pixel format.
		 */
		legacy::mali_gralloc_adjust_dimensions(legacy_internal_format,
		                                       usage,
		                                       legacy_alloc_type,
		                                       bufDescriptor->width,
		                                       bufDescriptor->height,
		                                       &bufDescriptor->old_alloc_width,
		                                       &bufDescriptor->old_alloc_height);

		size_t size = 0;
		int res = legacy::get_alloc_size(legacy_internal_format,
		                                 usage,
		                                 legacy_alloc_type,
		                                 bufDescriptor->old_alloc_width,
		                                 bufDescriptor->old_alloc_height,
		                                 &bufDescriptor->old_byte_stride,
		                                 &bufDescriptor->pixel_stride,
		                                 &size);
		if (res < 0)
		{
			//return res;
		}

		/*
		 * Accommodate for larger legacy allocation size.
		 */
		if (size > bufDescriptor->size)
		{
			bufDescriptor->size = size;
		}
#endif

		/*
		 * Each layer of a multi-layer buffer must be aligned so that
		 * it is accessible by both producer and consumer. In most cases,
		 * the stride alignment is also sufficient for each layer, however
		 * for AFBC the header buffer alignment is more constrained (see
		 * AFBC specification v3.4, section 2.15: "Alignment requirements").
		 * Also update the buffer size to accommodate all layers.
		 */
		if (bufDescriptor->layer_count > 1)
		{
			if (bufDescriptor->internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK)
			{
				if (bufDescriptor->internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
				{
					bufDescriptor->size = GRALLOC_ALIGN(bufDescriptor->size, 4096);
				}
				else
				{
					bufDescriptor->size = GRALLOC_ALIGN(bufDescriptor->size, 128);
				}
			}

			bufDescriptor->size *= bufDescriptor->layer_count;
		}
	}

	/* Allocate ION backing store memory */
	err = mali_gralloc_ion_allocate(m, descriptors, numDescriptors, pHandle, &shared);

	if (err < 0)
	{
		return err;
	}

	if (shared)
	{
		backing_store_id = getUniqueId();
	}

	for (uint32_t i = 0; i < numDescriptors; i++)
	{
		buffer_descriptor_t * const bufDescriptor = (buffer_descriptor_t *)descriptors[i];
		private_handle_t *hnd = (private_handle_t *)pHandle[i];
		uint64_t usage = bufDescriptor->consumer_usage | bufDescriptor->producer_usage;

		err = gralloc_buffer_attr_allocate(hnd);

		if (err < 0)
		{
			/* free all allocated ion buffer& attr buffer here.*/
			mali_gralloc_buffer_free_internal(pHandle, numDescriptors);
			return err;
		}

		mali_gralloc_dump_buffer_add(hnd);

		uint32_t format_idx;
		for (format_idx = 0; format_idx < num_formats; format_idx++)
		{
			if (formats[format_idx].id == (bufDescriptor->alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK))
			{
				break;
			}
		}

		if (formats[format_idx].is_yuv)
		{
			hnd->yuv_info = MALI_YUV_BT601_NARROW;
#if GRALLOC_USE_GRALLOC1_API == 1
			switch (usage & MALI_GRALLOC_USAGE_YUV_CONF_MASK)
			{
			case MALI_GRALLOC_USAGE_YUV_CONF_0:
				/* Covered by MALI_YUV_BT601_NARROW value assigned
				 * to yuv_info by default. */
				break;

			case MALI_GRALLOC_USAGE_YUV_CONF_1:
				hnd->yuv_info = MALI_YUV_BT601_WIDE;
				break;

			case MALI_GRALLOC_USAGE_YUV_CONF_2:
				hnd->yuv_info = MALI_YUV_BT709_NARROW;
				break;

			case MALI_GRALLOC_USAGE_YUV_CONF_3:
				hnd->yuv_info = MALI_YUV_BT709_WIDE;
				break;
			}
#endif

			/* Workaround 10bit YUV only support BT709_WIDE in GPU DDK */
			if (formats[format_idx].bps == 10)
			{
				hnd->yuv_info = MALI_YUV_BT709_WIDE;
			}
		}

		if (shared)
		{
			/*each buffer will share the same backing store id.*/
			hnd->backing_store_id = backing_store_id;
		}
		else
		{
			/* each buffer will have an unique backing store id.*/
			hnd->backing_store_id = getUniqueId();
		}
	}

	if (NULL != shared_backend)
	{
		*shared_backend = shared;
	}

	return 0;
}

int mali_gralloc_buffer_free(buffer_handle_t pHandle)
{
	int rval = -1;
	private_handle_t *hnd = (private_handle_t *)(pHandle);

	if (hnd != NULL)
	{
		rval = gralloc_buffer_attr_free(hnd);
		mali_gralloc_ion_free(hnd);
	}

	return rval;
}

static int mali_gralloc_buffer_free_internal(buffer_handle_t *pHandle, uint32_t num_hnds)
{
	int err = -1;
	uint32_t i = 0;

	for (i = 0; i < num_hnds; i++)
	{
		private_handle_t *hnd = (private_handle_t *)(pHandle[i]);

		err = gralloc_buffer_attr_free(hnd);
		mali_gralloc_ion_free(hnd);
	}

	return err;
}
