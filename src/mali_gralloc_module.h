/*
 * Copyright (C) 2016 ARM Limited. All rights reserved.
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
#ifndef MALI_GRALLOC_MODULE_H_
#define MALI_GRALLOC_MODULE_H_

#include <system/window.h>
#include <linux/fb.h>
#include <pthread.h>

#if GRALLOC_USE_LEGACY_ION_API != 1
#include <ion/ion_4.12.h>
#endif

typedef enum
{
	MALI_DPY_TYPE_UNKNOWN = 0,
	MALI_DPY_TYPE_CLCD,
	MALI_DPY_TYPE_HDLCD
} mali_dpy_type;


#if GRALLOC_USE_GRALLOC1_API == 1

typedef struct
{
	struct hw_module_t common;
} gralloc_module_t;

#endif

struct private_module_t
{
	gralloc_module_t base;

	struct private_handle_t *framebuffer;
	uint32_t flags;
	uint32_t numBuffers;
	uint32_t bufferMask;
	pthread_mutex_t lock;
	buffer_handle_t currentBuffer;
	int ion_client;
	mali_dpy_type dpy_type;

	struct fb_var_screeninfo info;
	struct fb_fix_screeninfo finfo;
	float xdpi;
	float ydpi;
	float fps;
	int swapInterval;
	bool use_legacy_ion;
	uint64_t fbdev_format;
	bool secure_heap_exists;

#if GRALLOC_USE_LEGACY_ION_API != 1
	/* Cache the heap types / IDs information to avoid repeated IOCTL calls
	 * Assumption: Heap types / IDs would not change after boot up. */
	int heap_cnt;
	ion_heap_data heap_info[ION_NUM_HEAP_IDS];
#endif

#ifdef __cplusplus
	/* Never intended to be used from C code */
	enum
	{
		// flag to indicate we'll post this buffer
		PRIV_USAGE_LOCKED_FOR_POST = 0x80000000
	};
#endif

#ifdef __cplusplus
	/* default constructor */
	private_module_t();
#endif
};
typedef struct private_module_t mali_gralloc_module;

#endif /* MALI_GRALLOC_MODULE_H_ */
