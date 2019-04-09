/*
 * COPYRIGHT (C) 2017-2018 ARM Limited. All rights reserved.
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

#ifndef MALI_GRALLOC_USAGES_H_
#define MALI_GRALLOC_USAGES_H_

/*
 * Below usage types overlap, this is intentional.
 * The reason is that for Gralloc 0.3 there are very
 * few usage flags we have at our disposal.
 *
 * The overlapping is handled by processing the definitions
 * in a specific order.
 *
 * MALI_GRALLOC_USAGE_PRIVATE_FORMAT and MALI_GRALLOC_USAGE_NO_AFBC
 * don't overlap and are processed first.
 *
 * MALI_GRALLOC_USAGE_YUV_CONF are only for YUV formats and clients
 * using MALI_GRALLOC_USAGE_NO_AFBC must never allocate YUV formats.
 * The latter is strictly enforced and allocations will fail.
 *
 * MALI_GRALLOC_USAGE_AFBC_PADDING is only valid if MALI_GRALLOC_USAGE_NO_AFBC
 * is not present.
 */

/*
 * Gralloc private usage 0-3 are the same in 0.3 and 1.0.
 * We defined based our usages based on what is available.
 */
#if defined(GRALLOC_MODULE_API_VERSION_1_0)

#define GRALLOC_USAGE_PRIVATE_MASK (0xffff0000f0000000U)

/*
 * Most gralloc code is fairly version agnostic, but certain
 * places still use old usage defines. Make sure it works
 * ok for usages that are backwards compatible.
 */
#define GRALLOC_USAGE_PRIVATE_0 GRALLOC1_CONSUMER_USAGE_PRIVATE_0
#define GRALLOC_USAGE_PRIVATE_1 GRALLOC1_CONSUMER_USAGE_PRIVATE_1
#define GRALLOC_USAGE_PRIVATE_2 GRALLOC1_CONSUMER_USAGE_PRIVATE_2
#define GRALLOC_USAGE_PRIVATE_3 GRALLOC1_CONSUMER_USAGE_PRIVATE_3

#define GRALLOC_USAGE_SW_WRITE_RARELY GRALLOC1_PRODUCER_USAGE_CPU_WRITE
#define GRALLOC_USAGE_SW_WRITE_OFTEN GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN
#define GRALLOC_USAGE_SW_READ_RARELY GRALLOC1_CONSUMER_USAGE_CPU_READ
#define GRALLOC_USAGE_SW_READ_OFTEN GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN
#define GRALLOC_USAGE_HW_FB GRALLOC1_CONSUMER_USAGE_CLIENT_TARGET
#define GRALLOC_USAGE_HW_2D 0x00000400

#define GRALLOC_USAGE_SW_WRITE_MASK 0x000000F0
#define GRALLOC_USAGE_SW_READ_MASK 0x0000000F
#define GRALLOC_USAGE_PROTECTED GRALLOC1_PRODUCER_USAGE_PROTECTED
#define GRALLOC_USAGE_HW_RENDER GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET
#define GRALLOC_USAGE_HW_CAMERA_MASK (GRALLOC1_CONSUMER_USAGE_CAMERA | GRALLOC1_PRODUCER_USAGE_CAMERA)
#define GRALLOC_USAGE_HW_TEXTURE GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE
#define GRALLOC_USAGE_HW_VIDEO_ENCODER GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER
#define GRALLOC_USAGE_HW_COMPOSER GRALLOC1_CONSUMER_USAGE_HWCOMPOSER
#define GRALLOC_USAGE_EXTERNAL_DISP 0x00002000

#define GRALLOC_USAGE_SENSOR_DIRECT_DATA GRALLOC1_PRODUCER_USAGE_SENSOR_DIRECT_DATA
#define GRALLOC_USAGE_GPU_DATA_BUFFER GRALLOC1_CONSUMER_USAGE_GPU_DATA_BUFFER


typedef enum
{
	/*
	 * Allocation will be used as a front-buffer, which
	 * supports concurrent producer-consumer access.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER
	 */
	MALI_GRALLOC_USAGE_FRONTBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_0,

	/*
	 * Allocation will be used as a back-buffer.
	 * Use when switching from front-buffer as a workaround for Android
	 * buffer queue, which does not re-allocate for a sub-set of
	 * existing usage.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FRONTBUFFER.
	 */
	MALI_GRALLOC_USAGE_FORCE_BACKBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_1,

	/*
	 * Buffer will not be allocated with AFBC.
	 *
	 * NOTE: Not compatible with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER so cannot be
	 * used when switching from front-buffer to back-buffer.
	 */
	MALI_GRALLOC_USAGE_NO_AFBC = (GRALLOC1_PRODUCER_USAGE_PRIVATE_1 | GRALLOC1_PRODUCER_USAGE_PRIVATE_2),

	/* Custom alignment for AFBC headers.
	 *
	 * NOTE: due to usage flag overlap, AFBC_PADDING cannot be used with FORCE_BACKBUFFER.
	 */
	MALI_GRALLOC_USAGE_AFBC_PADDING = GRALLOC1_PRODUCER_USAGE_PRIVATE_2,

	/* Private format usage.
	 * 'format' argument to allocation function will be interpreted in a
	 * private manner and must be constructed via GRALLOC_PRIVATE_FORMAT_WRAPPER_*
	 * macros which pack base format and AFBC format modifiers into 32-bit value.
	 */
	MALI_GRALLOC_USAGE_PRIVATE_FORMAT = GRALLOC1_PRODUCER_USAGE_PRIVATE_3,

	/* YUV-only. */
	MALI_GRALLOC_USAGE_YUV_CONF_0 = 0,
	MALI_GRALLOC_USAGE_YUV_CONF_1 = GRALLOC1_PRODUCER_USAGE_PRIVATE_18,
	MALI_GRALLOC_USAGE_YUV_CONF_2 = GRALLOC1_PRODUCER_USAGE_PRIVATE_19,
	MALI_GRALLOC_USAGE_YUV_CONF_3 = (GRALLOC1_PRODUCER_USAGE_PRIVATE_18 | GRALLOC1_PRODUCER_USAGE_PRIVATE_19),
	MALI_GRALLOC_USAGE_YUV_CONF_MASK = MALI_GRALLOC_USAGE_YUV_CONF_3,

} mali_gralloc_usage_type;


#elif defined(GRALLOC_MODULE_API_VERSION_0_3)

typedef enum
{
	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_FRONTBUFFER = GRALLOC_USAGE_PRIVATE_0,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_FORCE_BACKBUFFER = GRALLOC_USAGE_PRIVATE_1,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_NO_AFBC = (GRALLOC_USAGE_PRIVATE_1 | GRALLOC_USAGE_PRIVATE_2),

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_AFBC_PADDING = GRALLOC_USAGE_PRIVATE_2,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_PRIVATE_FORMAT = GRALLOC_USAGE_PRIVATE_3,

} mali_gralloc_usage_type;

#else


#if defined(GRALLOC_LIBRARY_BUILD)
#error "Please include mali_gralloc_module.h before including other gralloc headers when building gralloc itself"
#else
#error "Please include either gralloc.h or gralloc1.h header before including gralloc_priv.h"
#endif

#endif

#endif /*MALI_GRALLOC_USAGES_H_*/
