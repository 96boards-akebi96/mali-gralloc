/*
 * Copyright (C) 2016-2017 ARM Limited. All rights reserved.
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

#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>

#include <log/log.h>
#include <cutils/atomic.h>

#include <ion/ion.h>
#if GRALLOC_USE_LEGACY_ION_API != 1
#include <ion_4.12.h>
#include <vector>
#endif
#include <sys/ioctl.h>

#include <hardware/hardware.h>

#if GRALLOC_USE_GRALLOC1_API == 1
#include <hardware/gralloc1.h>
#else
#include <hardware/gralloc.h>
#endif

#include "mali_gralloc_module.h"
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_helper.h"
#include "framebuffer_device.h"
#include "mali_gralloc_formats.h"
#include "mali_gralloc_usages.h"
#include "mali_gralloc_bufferdescriptor.h"
#include "mali_gralloc_bufferallocation.h"

#define HEAP_MASK_FROM_ID(id) (1 << id)
#define HEAP_MASK_FROM_TYPE(type) (1 << type)

static const enum ion_heap_type ION_HEAP_TYPE_INVALID = ((enum ion_heap_type)~0);
static const enum ion_heap_type ION_HEAP_TYPE_SECURE = (enum ion_heap_type)(((unsigned int)ION_HEAP_TYPE_CUSTOM) + 1);

#if defined(ION_HEAP_SECURE_MASK)
#if (HEAP_MASK_FROM_TYPE(ION_HEAP_TYPE_SECURE) != ION_HEAP_SECURE_MASK)
#error "ION_HEAP_TYPE_SECURE value is not compatible with ION_HEAP_SECURE_MASK"
#endif
#endif

static void mali_gralloc_ion_free_internal(buffer_handle_t *pHandle, uint32_t num_hnds);
static void set_ion_flags(enum ion_heap_type heap_type, uint64_t usage,
                          unsigned int *priv_heap_flag, unsigned int *ion_flags);

#include "asm/ion_uniphier.h"

/*
 *  Identifies a heap and retrieves file descriptor from ION for allocation
 *
 * @param m         [in]    Gralloc module.
 * @param usage     [in]    Producer and consumer combined usage.
 * @param size      [in]    Requested buffer size (in bytes).
 * @param heap_type [in]    Requested heap type.
 * @param flags     [in]    ION allocation attributes defined by ION_FLAG_*.
 * @param min_pgsz  [out]   Minimum page size (in bytes).
 *
 * @return File handle which can be used for allocation, on success
 *         -1, otherwise.
 */
static int alloc_from_ion_heap(mali_gralloc_module *m, uint64_t usage, size_t size,
                               enum ion_heap_type heap_type, unsigned int flags,
                               int *min_pgsz)
{
	int shared_fd = -1;
	int ret = -1;

	if ((m->ion_client < 0) || (size <= 0) || (heap_type == ION_HEAP_TYPE_INVALID) ||
	    (min_pgsz == NULL))
	{
		return -1;
	}

#if GRALLOC_USE_LEGACY_ION_API != 1
	bool system_heap_exist = false;

	if (m->use_legacy_ion == false)
	{
		int i = 0;
		bool is_heap_matched = false;

		/* Attempt to allocate memory from each matching heap type (of
		 * enumerated heaps) until successful
		 */
		do
		{
			if (heap_type == m->heap_info[i].type)
			{
				is_heap_matched = true;
				ret = ion_alloc_fd(m->ion_client, size, 0,
				                   HEAP_MASK_FROM_ID(m->heap_info[i].heap_id),
				                   flags, &shared_fd);
			}

			if (m->heap_info[i].type == ION_HEAP_TYPE_SYSTEM)
			{
				system_heap_exist = true;
			}

			i++;
		} while ((ret < 0) && (i < m->heap_cnt));

		if (is_heap_matched == false)
		{
			AERR("Failed to find matching ION heap. Trying to fall back on system heap");
		}
	}
	else
#endif
	{
		/* This assumes that when the heaps were defined, the heap ids were
		 * defined as (1 << type) and that ION interprets the heap_mask as
		 * (1 << type).
		 */
		unsigned int heap_mask = HEAP_MASK_FROM_TYPE(heap_type);

		ret = ion_alloc_fd(m->ion_client, size, 0, heap_mask, flags, &shared_fd);
	}

	/* Check if allocation from selected heap failed and fall back to system
	 * heap if possible.
	 */
	if (ret < 0)
	{
		/* Don't allow falling back to sytem heap if secure was requested. */
		if (heap_type == ION_HEAP_TYPE_SECURE)
		{
			return -1;
		}

		/* Can't fall back to system heap if system heap was the heap that
		 * already failed
		 */
		if (heap_type == ION_HEAP_TYPE_SYSTEM)
		{
			AERR("%s: Allocation failed on on system heap. Cannot fallback.", __func__);
			return -1;
		}

		heap_type = ION_HEAP_TYPE_SYSTEM;

		/* Set ION flags for system heap allocation */
		set_ion_flags(heap_type, usage, NULL, &flags);

#if GRALLOC_USE_LEGACY_ION_API != 1
		if (m->use_legacy_ion == false)
		{
			int i = 0;

			if (system_heap_exist == false)
			{
				AERR("%s: System heap not available for fallback", __func__);
				return -1;
			}

			/* Attempt to allocate memory from each system heap type (of
			 * enumerated heaps) until successful
			 */
			do
			{
				if (m->heap_info[i].type == ION_HEAP_TYPE_SYSTEM)
				{
					ret = ion_alloc_fd(m->ion_client, size, 0,
					                   HEAP_MASK_FROM_ID(m->heap_info[i].heap_id),
					                   flags, &shared_fd);
				}

				i++;
			} while ((ret < 0) && (i < m->heap_cnt));
		}
		else /* Use legacy ION API */
#endif
		{
			ret = ion_alloc_fd(m->ion_client, size, 0,
			                   HEAP_MASK_FROM_TYPE(heap_type),
			                   flags, &shared_fd);
		}

		if (ret != 0)
		{
			AERR("Fallback ion_alloc_fd(%d, %zd, %d, %u, %p) failed",
			      m->ion_client, size, 0, flags, &shared_fd);
			return -1;
		}
	}

	switch (heap_type)
	{
	case ION_HEAP_TYPE_SYSTEM:
		*min_pgsz = SZ_4K;
		break;

	case ION_HEAP_TYPE_SYSTEM_CONTIG:
	case ION_HEAP_TYPE_CARVEOUT:
#if GRALLOC_USE_ION_DMA_HEAP
	case ION_HEAP_TYPE_DMA:
		*min_pgsz = size;
		break;
#endif
#if GRALLOC_USE_ION_COMPOUND_PAGE_HEAP
	case ION_HEAP_TYPE_COMPOUND_PAGE:
		*min_pgsz = SZ_2M;
		break;
#endif
	/* If have customized heap please set the suitable pg type according to
	 * the customized ION implementation
	 */
	case ION_HEAP_TYPE_CUSTOM:
		*min_pgsz = SZ_4K;
		break;
	default:
		*min_pgsz = SZ_4K;
		break;
	}

	return shared_fd;
}

static enum ion_heap_type pick_ion_heap(const mali_gralloc_module * const m, uint64_t usage)
{
	enum ion_heap_type heap_type = ION_HEAP_TYPE_INVALID;

	if (usage & GRALLOC_USAGE_PROTECTED)
	{
		if (m->secure_heap_exists)
		{
			heap_type = ION_HEAP_TYPE_SECURE;
		}
		else
		{
			AERR("Protected ION memory is not supported on this platform.");
		}
	}
	else if (usage & GRALLOC_USAGE_HW_FB)
	{
		heap_type = (ion_heap_type)ION_HEAP_ID_FB;
	}
	else if (!(usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) && (usage & (GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_COMPOSER)))
	{
#if GRALLOC_USE_ION_COMPOUND_PAGE_HEAP
		heap_type = ION_HEAP_TYPE_COMPOUND_PAGE;
#elif GRALLOC_USE_ION_DMA_HEAP
		heap_type = ION_HEAP_TYPE_DMA;
#else
		heap_type = ION_HEAP_TYPE_SYSTEM;
#endif
	}
	else
	{
		heap_type = ION_HEAP_TYPE_SYSTEM;
	}

	return heap_type;
}

static void set_ion_flags(enum ion_heap_type heap_type, uint64_t usage,
                          unsigned int *priv_heap_flag, unsigned int *ion_flags)
{
#if !GRALLOC_USE_ION_DMA_HEAP
	GRALLOC_UNUSED(heap_type);
#endif

	if (priv_heap_flag)
	{
#if GRALLOC_USE_ION_DMA_HEAP
		if (heap_type == ION_HEAP_TYPE_DMA)
		{
			*priv_heap_flag = private_handle_t::PRIV_FLAGS_USES_ION_DMA_HEAP;
		}
#endif
	}

	if (ion_flags)
	{
#if GRALLOC_USE_ION_DMA_HEAP
		if (heap_type != ION_HEAP_TYPE_DMA)
		{
#endif
			if ((usage & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN)
			{
				*ion_flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
			}
#if GRALLOC_USE_ION_DMA_HEAP
		}
#endif
	}
}

static bool check_buffers_sharable(const mali_gralloc_module * const m,
                                   const gralloc_buffer_descriptor_t *descriptors,
                                   uint32_t numDescriptors)
{
	enum ion_heap_type shared_backend_heap_type = ION_HEAP_TYPE_INVALID;
	unsigned int shared_ion_flags = 0;
	uint64_t usage;
	uint32_t i;

	if (numDescriptors <= 1)
	{
		return false;
	}

	for (i = 0; i < numDescriptors; i++)
	{
		unsigned int ion_flags;
		enum ion_heap_type heap_type;

		buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)descriptors[i];

		usage = bufDescriptor->consumer_usage | bufDescriptor->producer_usage;

		heap_type = pick_ion_heap(m, usage);
		if (heap_type == ION_HEAP_TYPE_INVALID)
		{
			return false;
		}

		set_ion_flags(heap_type, usage, NULL, &ion_flags);

		if (shared_backend_heap_type != ION_HEAP_TYPE_INVALID)
		{
			if (shared_backend_heap_type != heap_type || shared_ion_flags != ion_flags)
			{
				return false;
			}
		}
		else
		{
			shared_backend_heap_type = heap_type;
			shared_ion_flags = ion_flags;
		}
	}

	return true;
}

static int get_max_buffer_descriptor_index(const gralloc_buffer_descriptor_t *descriptors, uint32_t numDescriptors)
{
	uint32_t i, max_buffer_index = 0;
	size_t max_buffer_size = 0;

	for (i = 0; i < numDescriptors; i++)
	{
		buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)descriptors[i];

		if (max_buffer_size < bufDescriptor->size)
		{
			max_buffer_index = i;
			max_buffer_size = bufDescriptor->size;
		}
	}

	return max_buffer_index;
}

/*
 * Opens the ION module. Queries heap information and stores it for later use
 *
 * @param m  [inout]    Gralloc private module
 *
 * @return              0 in case of success
 *                      -1 for all error cases
 */
static int open_and_query_ion(mali_gralloc_module *m)
{
	int ret = -1;

	m->ion_client = ion_open();
	if (m->ion_client < 0)
	{
		AERR("ion_open failed with %s", strerror(errno));
		return -1;
	}

#if GRALLOC_USE_LEGACY_ION_API == 1
	m->use_legacy_ion = true;
#else
	int heap_cnt = 0;

	m->heap_cnt = 0;
	m->use_legacy_ion = (ion_is_legacy(m->ion_client) != 0);

	if (m->use_legacy_ion == false)
	{
		ret = ion_query_heap_cnt(m->ion_client, &heap_cnt);
		if (ret == 0)
		{
			if (heap_cnt > (int)ION_NUM_HEAP_IDS)
			{
				AERR("Retrieved heap count %d is more than maximun heaps %zu on ion",
				      heap_cnt, ION_NUM_HEAP_IDS);
				return -1;
			}

			std::vector<struct ion_heap_data> heap_data(heap_cnt);
			ret = ion_query_get_heaps(m->ion_client, heap_cnt, heap_data.data());
			if (ret == 0)
			{
				int heap_info_idx = 0;
				for (std::vector<struct ion_heap_data>::iterator heap = heap_data.begin();
                                            heap != heap_data.end(); heap++)
				{
					if (heap_info_idx >= (int)ION_NUM_HEAP_IDS)
					{
						AERR("Iterator exceeding max index, cannot cache heap information");
						return -1;
					}

					if (strcmp(heap->name, "ion_protected_heap") == 0)
					{
						heap->type = ION_HEAP_TYPE_SECURE;
						m->secure_heap_exists = true;
					}

					m->heap_info[heap_info_idx] = *heap;
					heap_info_idx++;
				}
			}
		}
		if (ret < 0)
		{
			AERR("%s: Failed to query ION heaps.", __func__);
			return ret;
		}

		m->heap_cnt = heap_cnt;
	}
	else
#endif
	{
#if defined(ION_HEAP_SECURE_MASK)
		m->secure_heap_exists = true;
#endif
	}

	return 0;
}

/*
 *  Allocates ION buffers
 *
 * @param m               [in]    Gralloc module.
 * @param descriptors     [in]    Buffer request descriptors
 * @param numDescriptors  [in]    Number of descriptors
 * @param pHandle         [out]   Handle for each allocated buffer
 * @param shared_backend  [out]   Shared buffers flag
 *
 * @return File handle which can be used for allocation, on success
 *         -1, otherwise.
 */
int mali_gralloc_ion_allocate(mali_gralloc_module *m,
                              const gralloc_buffer_descriptor_t *descriptors,
                              uint32_t numDescriptors, buffer_handle_t *pHandle,
                              bool *shared_backend)
{
	static int support_protected = 1; /* initially, assume we support protected memory */
	unsigned int priv_heap_flag = 0;
	enum ion_heap_type heap_type;
	unsigned char *cpu_ptr = NULL;
	uint64_t usage;
	uint32_t i, max_buffer_index = 0;
	int shared_fd;
	unsigned int ion_flags = 0;
	int min_pgsz = 0;

	if (m->ion_client < 0)
	{
		int status = 0;
		status = open_and_query_ion(m);
		if (status < 0)
		{
			return status;
		}
	}

	*shared_backend = check_buffers_sharable(m, descriptors, numDescriptors);

	if (*shared_backend)
	{
		buffer_descriptor_t *max_bufDescriptor;

		max_buffer_index = get_max_buffer_descriptor_index(descriptors, numDescriptors);
		max_bufDescriptor = (buffer_descriptor_t *)(descriptors[max_buffer_index]);
		usage = max_bufDescriptor->consumer_usage | max_bufDescriptor->producer_usage;

		heap_type = pick_ion_heap(m, usage);
		if (heap_type == ION_HEAP_TYPE_INVALID)
		{
			AERR("Failed to find an appropriate ion heap");
			return -1;
		}

		set_ion_flags(heap_type, usage, &priv_heap_flag, &ion_flags);

		shared_fd = alloc_from_ion_heap(m, usage, max_bufDescriptor->size, heap_type, ion_flags, &min_pgsz);

		if (shared_fd < 0)
		{
			AERR("ion_alloc failed form client: ( %d )", m->ion_client);
			return -1;
		}

		for (i = 0; i < numDescriptors; i++)
		{
			buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)(descriptors[i]);
			int tmp_fd;

			if (i != max_buffer_index)
			{
				tmp_fd = dup(shared_fd);

				if (tmp_fd < 0)
				{
					AERR("Ion shared fd:%d of index:%d could not be duplicated for descriptor:%d",
					      shared_fd, max_buffer_index, i);

					/* It is possible that already opened shared_fd for the
					 * max_bufDescriptor is also not closed */
					if (i < max_buffer_index)
					{
						close(shared_fd);
					}

					/* Need to free already allocated memory. */
					mali_gralloc_ion_free_internal(pHandle, numDescriptors);
					return -1;
				}
			}
			else
			{
				tmp_fd = shared_fd;
			}

			private_handle_t *hnd = new private_handle_t(
			    private_handle_t::PRIV_FLAGS_USES_ION | priv_heap_flag, bufDescriptor->size, min_pgsz,
			    bufDescriptor->consumer_usage, bufDescriptor->producer_usage, tmp_fd, bufDescriptor->hal_format,
			    bufDescriptor->internal_format, bufDescriptor->alloc_format,
			    bufDescriptor->width, bufDescriptor->height, bufDescriptor->pixel_stride,
			    bufDescriptor->old_alloc_width, bufDescriptor->old_alloc_height, bufDescriptor->old_byte_stride,
			    max_bufDescriptor->size, bufDescriptor->layer_count, bufDescriptor->plane_info);

			if (NULL == hnd)
			{
				AERR("Private handle could not be created for descriptor:%d of shared usecase", i);

				/* Close the obtained shared file descriptor for the current handle */
				close(tmp_fd);

				/* It is possible that already opened shared_fd for the
				 * max_bufDescriptor is also not closed */
				if (i < max_buffer_index)
				{
					close(shared_fd);
				}

				/* Free the resources allocated for the previous handles */
				mali_gralloc_ion_free_internal(pHandle, numDescriptors);
				return -1;
			}

			pHandle[i] = hnd;
		}
	}
	else
	{
		for (i = 0; i < numDescriptors; i++)
		{
			buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)(descriptors[i]);
			usage = bufDescriptor->consumer_usage | bufDescriptor->producer_usage;

			heap_type = pick_ion_heap(m, usage);
			if (heap_type == ION_HEAP_TYPE_INVALID)
			{
				AERR("Failed to find an appropriate ion heap");
				mali_gralloc_ion_free_internal(pHandle, numDescriptors);
				return -1;
			}

			set_ion_flags(heap_type, usage, &priv_heap_flag, &ion_flags);

			shared_fd = alloc_from_ion_heap(m, usage, bufDescriptor->size, heap_type, ion_flags, &min_pgsz);

			if (shared_fd < 0)
			{
				AERR("ion_alloc failed from client ( %d )", m->ion_client);

				/* need to free already allocated memory. not just this one */
				mali_gralloc_ion_free_internal(pHandle, numDescriptors);

				return -1;
			}

			private_handle_t *hnd = new private_handle_t(
			    private_handle_t::PRIV_FLAGS_USES_ION | priv_heap_flag, bufDescriptor->size, min_pgsz,
			    bufDescriptor->consumer_usage, bufDescriptor->producer_usage, shared_fd, bufDescriptor->hal_format,
			    bufDescriptor->internal_format, bufDescriptor->alloc_format,
			    bufDescriptor->width, bufDescriptor->height, bufDescriptor->pixel_stride,
			    bufDescriptor->old_alloc_width, bufDescriptor->old_alloc_height, bufDescriptor->old_byte_stride,
			    bufDescriptor->size, bufDescriptor->layer_count, bufDescriptor->plane_info);

			if (NULL == hnd)
			{
				AERR("Private handle could not be created for descriptor:%d in non-shared usecase", i);

				/* Close the obtained shared file descriptor for the current handle */
				close(shared_fd);
				mali_gralloc_ion_free_internal(pHandle, numDescriptors);
				return -1;
			}

			pHandle[i] = hnd;
		}
	}

	for (i = 0; i < numDescriptors; i++)
	{
		buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)(descriptors[i]);
		private_handle_t *hnd = (private_handle_t *)(pHandle[i]);

		usage = bufDescriptor->consumer_usage | bufDescriptor->producer_usage;

		if (!(usage & GRALLOC_USAGE_PROTECTED))
		{
			cpu_ptr =
			    (unsigned char *)mmap(NULL, bufDescriptor->size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_fd, 0);

			if (MAP_FAILED == cpu_ptr)
			{
				AERR("mmap failed from client ( %d ), fd ( %d )", m->ion_client, hnd->share_fd);
				mali_gralloc_ion_free_internal(pHandle, numDescriptors);
				return -1;
			}

#if GRALLOC_INIT_AFBC == 1
			if ((bufDescriptor->internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK) && (!(*shared_backend)))
			{
				/* For separated plane YUV, there is a header to initialise per plane. */
				const plane_info_t *plane_info = bufDescriptor->plane_info;
				const bool is_multi_plane = hnd->is_multi_plane();
				for (int i = 0; i < MAX_PLANES && (i == 0 || plane_info[i].byte_stride != 0); i++)
				{
					init_afbc(cpu_ptr + plane_info[i].offset,
					          bufDescriptor->internal_format,
					          is_multi_plane,
					          plane_info[i].alloc_width,
					          plane_info[i].alloc_height);
				}
			}
#endif
			hnd->base = cpu_ptr;
		}
	}

	return 0;
}

void mali_gralloc_ion_free(private_handle_t const *hnd)
{
	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
	{
		return;
	}
	else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
		/* Buffer might be unregistered already so we need to assure we have a valid handle*/
		if (0 != hnd->base)
		{
			if (0 != munmap((void *)hnd->base, hnd->size))
			{
				AERR("Failed to munmap handle %p", hnd);
			}
		}

		close(hnd->share_fd);
		memset((void *)hnd, 0, sizeof(*hnd));
	}
}

static void mali_gralloc_ion_free_internal(buffer_handle_t *pHandle, uint32_t num_hnds)
{
	uint32_t i = 0;

	for (i = 0; i < num_hnds; i++)
	{
		if (NULL != pHandle[i])
		{
			mali_gralloc_ion_free((private_handle_t *)(pHandle[i]));
		}
	}

	return;
}

void mali_gralloc_ion_sync(const mali_gralloc_module *m, private_handle_t *hnd)
{
	if (m != NULL && hnd != NULL)
	{
		switch (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
		{
		case private_handle_t::PRIV_FLAGS_USES_ION:
			if (!(hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION_DMA_HEAP))
			{
				ion_sync_fd(m->ion_client, hnd->share_fd);
			}

			break;
		}
	}
}

int mali_gralloc_ion_map(private_handle_t *hnd)
{
	int retval = -EINVAL;

	switch (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
	case private_handle_t::PRIV_FLAGS_USES_ION:
		unsigned char *mappedAddress;
		size_t size = hnd->size;
		hw_module_t *pmodule = NULL;
		private_module_t *m = NULL;

		if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&pmodule) == 0)
		{
			m = reinterpret_cast<private_module_t *>(pmodule);
		}
		else
		{
			AERR("Could not get gralloc module for handle: %p", hnd);
			retval = -errno;
			break;
		}

		/* the test condition is set to m->ion_client <= 0 here, because:
		 * 1) module structure are initialized to 0 if no initial value is applied
		 * 2) a second user process should get a ion fd greater than 0.
		 */
		if (m->ion_client <= 0)
		{
			/* a second user process must obtain a client handle first via ion_open before it can obtain the shared ion buffer*/
			int status = 0;
			status = open_and_query_ion(m);
			if (status < 0)
			{
				return status;
			}
		}

		mappedAddress = (unsigned char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_fd, 0);

		if (MAP_FAILED == mappedAddress)
		{
			AERR("mmap( share_fd:%d ) failed with %s", hnd->share_fd, strerror(errno));
			retval = -errno;
			break;
		}

		hnd->base = (void *)(uintptr_t(mappedAddress) + hnd->offset);
		retval = 0;
		break;
	}

	return retval;
}

void mali_gralloc_ion_unmap(private_handle_t *hnd)
{
	switch (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
	case private_handle_t::PRIV_FLAGS_USES_ION:
		void *base = (void *)hnd->base;
		size_t size = hnd->size;

		if (munmap(base, size) < 0)
		{
			AERR("Could not munmap base:%p size:%zd '%s'", base, size, strerror(errno));
		}

		break;
	}
}

int mali_gralloc_ion_device_close(struct hw_device_t *device)
{
#if GRALLOC_USE_GRALLOC1_API == 1
	gralloc1_device_t *dev = reinterpret_cast<gralloc1_device_t *>(device);
#else
	alloc_device_t *dev = reinterpret_cast<alloc_device_t *>(device);
#endif

	if (dev)
	{
		private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);

		if (m->ion_client != -1)
		{
			if (0 != ion_close(m->ion_client))
			{
				AERR("Failed to close ion_client: %d err=%s", m->ion_client, strerror(errno));
			}

			m->ion_client = -1;
		}

		delete dev;
	}

	return 0;
}
