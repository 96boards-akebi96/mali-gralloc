/*
 * Copyright (C) 2016, 2018 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
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
#include <hardware/hardware.h>
#include <hardware/gralloc1.h>

#include "mali_gralloc_module.h"

#include "mali_gralloc_private_interface.h"
#include "mali_gralloc_buffer.h"
#include "mali_gralloc_ion.h"
#include "mali_gralloc_bufferdescriptor.h"
#include "mali_gralloc_bufferallocation.h"
#include "mali_gralloc_reference.h"
#include "mali_gralloc_bufferaccess.h"
#include "framebuffer_device.h"
#include "gralloc_buffer_priv.h"
#include "mali_gralloc_debug.h"

typedef struct mali_gralloc_func
{
	gralloc1_function_descriptor_t desc;
	gralloc1_function_pointer_t func;
} mali_gralloc_func;

static void mali_gralloc_dump(gralloc1_device_t *device, uint32_t *outSize, char *outBuffer)
{
	if (NULL == outSize)
	{
		ALOGE("Invalid pointer to outSize and return");
		return;
	}

	mali_gralloc_dump_internal(outSize, outBuffer);
	GRALLOC_UNUSED(device);
}

static int32_t mali_gralloc_create_descriptor(gralloc1_device_t *device, gralloc1_buffer_descriptor_t *outDescriptor)
{
	int ret = 0;
	ret = mali_gralloc_create_descriptor_internal(outDescriptor);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_destroy_descriptor(gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor)
{
	int ret = 0;
	ret = mali_gralloc_destroy_descriptor_internal(descriptor);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_set_consumer_usage(gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor,
                                               /*uint64_t */ gralloc1_consumer_usage_t usage)
{
	int ret = 0;
	ret = mali_gralloc_set_consumerusage_internal(descriptor, usage);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_set_dimensions(gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor,
                                           uint32_t width, uint32_t height)
{
	int ret = 0;
	ret = mali_gralloc_set_dimensions_internal(descriptor, width, height);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_set_format(gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor,
                                       /*int32_t*/ android_pixel_format_t format)
{
	int ret = 0;
	ret = mali_gralloc_set_format_internal(descriptor, format);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_set_producer_usage(gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor,
                                               /*uint64_t */ gralloc1_producer_usage_t usage)
{
	int ret = 0;
	ret = mali_gralloc_set_producerusage_internal(descriptor, usage);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_backing_store(gralloc1_device_t *device, buffer_handle_t buffer,
                                              gralloc1_backing_store_t *outStore)
{
	int ret = 0;
	ret = mali_gralloc_get_backing_store_internal(buffer, outStore);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_consumer_usage(gralloc1_device_t *device, buffer_handle_t buffer,
                                               uint64_t * /*gralloc1_consumer_usage_t*/ outUsage)
{
	int ret = 0;
	ret = mali_gralloc_get_consumer_usage_internal(buffer, outUsage);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_dimensions(gralloc1_device_t *device, buffer_handle_t buffer, uint32_t *outWidth,
                                           uint32_t *outHeight)
{
	int ret = 0;
	ret = mali_gralloc_get_dimensions_internal(buffer, outWidth, outHeight);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_format(gralloc1_device_t *device, buffer_handle_t buffer, int32_t *outFormat)
{
	int ret = 0;
	ret = mali_gralloc_get_format_internal(buffer, outFormat);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_producer_usage(gralloc1_device_t *device, buffer_handle_t buffer,
                                               uint64_t * /*gralloc1_producer_usage_t*/ outUsage)
{
	int ret = 0;
	ret = mali_gralloc_get_producer_usage_internal(buffer, outUsage);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc_get_stride(gralloc1_device_t *device, buffer_handle_t buffer, uint32_t *outStride)
{
	GRALLOC_UNUSED(device);
	int stride;

	const int ret = mali_gralloc_query_getstride(buffer, &stride);
	if (ret == GRALLOC1_ERROR_NONE)
	{
		*outStride = (uint32_t)stride;
	}

	return ret;
}

static int32_t mali_gralloc_allocate(gralloc1_device_t *device, uint32_t numDescriptors,
                                     const gralloc1_buffer_descriptor_t *descriptors, buffer_handle_t *outBuffers)
{
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);
	buffer_descriptor_t *bufDescriptor = (buffer_descriptor_t *)(*descriptors);
	uint64_t usage;
	bool shared = false;

	/* Initialise output parameters. */
	for (uint32_t i = 0; i < numDescriptors; i++)
	{
		outBuffers[i] = NULL;
	}

	usage = bufDescriptor->producer_usage | bufDescriptor->consumer_usage;

#if DISABLE_FRAMEBUFFER_HAL != 1

	if (usage & GRALLOC_USAGE_HW_FB)
	{
		int byte_stride;
		int pixel_stride;
		int width, height;
		uint64_t format;

		format = bufDescriptor->hal_format;
		width = bufDescriptor->width;
		height = bufDescriptor->height;

#if GRALLOC_FB_SWAP_RED_BLUE == 1
#if GRALLOC_FB_BPP == 16
		format = HAL_PIXEL_FORMAT_RGB_565;
#elif GRALLOC_FB_BPP == 32
		format = HAL_PIXEL_FORMAT_BGRA_8888;
#else
#error "Invalid framebuffer bit depth"
#endif
#endif

		if (fb_alloc_framebuffer(m, bufDescriptor->consumer_usage, bufDescriptor->producer_usage, outBuffers,
		                         &pixel_stride, &byte_stride) < 0)
		{
			return GRALLOC1_ERROR_NO_RESOURCES;
		}
		else
		{
			private_handle_t *hnd = (private_handle_t *)*outBuffers;

			/* Allocate a meta-data buffer for framebuffer too. fbhal
			 * ones wont need it but for hwc they will.
			 *
			 * Explicitly ignore allocation errors since it is not critical to have
			 */
			(void)gralloc_buffer_attr_allocate(hnd);

			hnd->req_format = format;
			hnd->yuv_info = MALI_YUV_BT601_NARROW;
			hnd->internal_format = format;
			hnd->alloc_format = format;
			hnd->byte_stride = byte_stride;
			hnd->width = width;
			hnd->height = height;
			hnd->stride = pixel_stride;
			hnd->internalWidth = width;
			hnd->internalHeight = height;
		}
	}
	else
#endif
	{
		if (mali_gralloc_buffer_allocate(m, (gralloc_buffer_descriptor_t *)descriptors, numDescriptors, outBuffers,
		                                 &shared) < 0)
		{
			ALOGE("Failed to allocate buffer.");
			return GRALLOC1_ERROR_NO_RESOURCES;
		}

		if (!shared && 1 != numDescriptors)
		{
			return GRALLOC1_ERROR_NOT_SHARED;
		}
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t mali_gralloc_retain(gralloc1_device_t *device, buffer_handle_t buffer)
{
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (private_handle_t::validate(buffer) < 0)
	{
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (mali_gralloc_reference_retain(m, buffer) < 0)
	{
		return GRALLOC1_ERROR_NO_RESOURCES;
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t mali_gralloc_release(gralloc1_device_t *device, buffer_handle_t buffer)
{
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (mali_gralloc_reference_release(m, buffer, true) < 0)
	{
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	return GRALLOC1_ERROR_NONE;
}

/*
 *  Returns the number of flex layout planes which are needed to represent the
 *  given buffer.
 *
 * @param device       [in]   Gralloc device.
 * @param buffer       [in]   The buffer handle for which the number of planes should be queried
 * @param outNumPlanes [out]  The number of flex planes required to describe the given buffer
 *
 * @return GRALLOC1_ERROR_NONE - The locking is successful;
 *         GRALLOC1_ERROR_BAD_HANDLE - The buffer handle is invalid
 *         GRALLOC1_ERROR_BAD_VALUE - The flex plane pointer is invalid
 *         Appropriate error, otherwise
 */
static int32_t mali_gralloc1_get_num_flex_planes(const gralloc1_device_t * const device,
                                                 const buffer_handle_t buffer,
                                                 uint32_t * const outNumPlanes)
{
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (private_handle_t::validate(buffer) < 0)
	{
		AERR("Invalid buffer handle");
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (outNumPlanes == NULL)
	{
		AERR("Invalid flex plane pointer");
		return GRALLOC1_ERROR_BAD_VALUE;
	}

	int status = mali_gralloc_get_num_flex_planes(m, buffer, outNumPlanes);
	if (status != 0)
	{
		return status;
	}

	return GRALLOC1_ERROR_NONE;
}

/*
 *  Tests buffer lock usage for Gralloc 1.0 API
 *
 * @param producerUsage [in]    The producer usage flags for lock (gralloc1_producer_usage_t).
 * @param consumerUsage [in]    The consumer usage flags for lock (gralloc1_consumer_usage_t).
 *
 * @return GRALLOC1_ERROR_NONE, when lock usage is valid;
 *         appropriate error, otherwise
 */
static int mali_gralloc1_test_lock_usage(const uint64_t producerUsage,
                                         const uint64_t consumerUsage)
{
#if GRALLOC_USE_LEGACY_LOCK != 1
	/* Validate the usage for
	 * 1. Exactly one of producerUsage and consumerUsage must be *_USAGE_NONE.
	 * 2. The usage which is not *_USAGE_NONE must be one of the *_USAGE_CPU_*
	 *    as applicable.
	 */
	if (producerUsage == GRALLOC1_PRODUCER_USAGE_NONE)
	{
		/* Consumer usage must be for CPU read */
		if ((consumerUsage & GRALLOC_USAGE_SW_READ_MASK) == 0)
		{
			AERR("The usage which is not *_USAGE_NONE must be one of the *_USAGE_CPU_*"
			     "producerUsage:0x%" PRIx64" consumerUsage:0x%" PRIx64, producerUsage,
			      consumerUsage);
			return GRALLOC1_ERROR_BAD_VALUE;
		}
	}
	else if (consumerUsage == GRALLOC1_CONSUMER_USAGE_NONE)
	{
		/* Producer usage can either be CPU read or CPU write */
		if ((producerUsage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)) == 0)
		{
			AERR("The usage which is not *_USAGE_NONE must be one of the *_USAGE_CPU_*"
			     "producerUsage:0x%" PRIx64" consumerUsage:0x%" PRIx64, producerUsage,
			      consumerUsage);
			return GRALLOC1_ERROR_BAD_VALUE;
		}
	}
	else
	{
		AERR("Exactly one of producerUsage and consumerUsage must be *_USAGE_NONE"
		     "producerUsage:0x%" PRIx64" consumerUsage:0x%" PRIx64, producerUsage,
		     consumerUsage);
		return GRALLOC1_ERROR_BAD_VALUE;
	}
#else
	if (!((producerUsage | consumerUsage) & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)))
	{
		return GRALLOC1_ERROR_BAD_VALUE;
	}
#endif
	return GRALLOC1_ERROR_NONE;
}

/*
 *  Locks the Gralloc 1.0 buffer for the specified CPU usage.
 *
 * @param device        [in]    Gralloc1 device.
 * @param buffer        [in]    The buffer to lock.
 * @param producerUsage [in]    The producer usage flags to request (gralloc1_producer_usage_t).
 * @param consumerUsage [in]    The consumer usage flags to request (gralloc1_consumer_usage_t).
 * @param accessRegion  [in]    The portion of the buffer that the client
 *                              intends to access.
 * @param outData       [out]   To be filled with a CPU-accessible pointer to
 *                              the buffer data for CPU usage.
 * @param acquireFence  [in]    Refers to an acquire sync fence object.
 *
 * @return GRALLOC1_ERROR_NONE, when locking is successful;
 *         appropriate error, otherwise
 *
 * @Notes: Locking a buffer simultaneously for write or read/write leaves the
 *         buffer's content into an indeterminate state.
 */
static int32_t mali_gralloc1_lock_async(gralloc1_device_t *device,
                                        buffer_handle_t buffer,
                                        uint64_t producerUsage,
                                        uint64_t consumerUsage,
                                        const gralloc1_rect_t *accessRegion,
                                        void **outData,
                                        int32_t acquireFence)
{
	int status = 0;
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (private_handle_t::validate(buffer) < 0)
	{
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (accessRegion == NULL)
	{
		return GRALLOC1_ERROR_BAD_VALUE;
	}

	status = mali_gralloc1_test_lock_usage(producerUsage, consumerUsage);
	if (status != GRALLOC1_ERROR_NONE)
	{
		return status;
	}

	status = mali_gralloc_lock_async(m, buffer, producerUsage | consumerUsage,
	                                 accessRegion->left, accessRegion->top,
	                                 accessRegion->width, accessRegion->height,
	                                 outData, acquireFence);
	if (status != 0)
	{
		if (status == -EINVAL)
		{
			return GRALLOC1_ERROR_BAD_VALUE;
		}

		return GRALLOC1_ERROR_UNSUPPORTED;
	}

	return GRALLOC1_ERROR_NONE;
}

/*
 *  Locks the Gralloc 1.0 buffer, for the specified CPU usage, asynchronously.
 *
 * @param device        [in]    Gralloc1 device.
 * @param buffer        [in]    The buffer to lock.
 * @param producerUsage [in]    The producer usage flags requested (gralloc1_producer_usage_t).
 * @param consumerUsage [in]    The consumer usage flags requested (gralloc1_consumer_usage_t).
 * @param accessRegion  [in]    The portion of the buffer that the client
 *                              intends to access.
 * @param outFlexLayout [out]   To be filled with the description of the planes
 *                              in the buffer
 * @param acquireFence  [in]    Refers to an acquire sync fence object.
 *
 * @return GRALLOC1_ERROR_NONE - Locking is successful;
 *         GRALLOC1_ERROR_BAD_HANDLE - Invalid buffer handle
 *         GRALLOC1_ERROR_BAD_VALUE - Invalid usage parameters
 *         GRALLOC1_ERROR_UNSUPPORTED - Any other error, like wrong access
 *                                      region parameters, invalid buffer ownership, etc.
 *
 * @Notes: Locking a buffer simultaneously for write or read/write leaves the
 *         buffer's content into an indeterminate state.
 */
static int32_t mali_gralloc1_lock_flex_async(gralloc1_device_t *device, buffer_handle_t buffer,
                                             uint64_t producerUsage,
                                             uint64_t consumerUsage,
                                             const gralloc1_rect_t *accessRegion,
                                             struct android_flex_layout *outFlexLayout,
                                             int32_t acquireFence)
{
	int status = 0;
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (private_handle_t::validate(buffer) < 0)
	{
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (outFlexLayout == NULL)
	{
		return GRALLOC1_ERROR_BAD_VALUE;
	}

	status = mali_gralloc1_test_lock_usage(producerUsage, consumerUsage);
	if (status != GRALLOC1_ERROR_NONE)
	{
		return status;
	}

	status = mali_gralloc_lock_flex_async(m, buffer, producerUsage | consumerUsage,
	                                      accessRegion->left, accessRegion->top,
	                                      accessRegion->width, accessRegion->height,
	                                      outFlexLayout, acquireFence);
	if (status != 0)
	{
		if (status == -EINVAL)
		{
			return GRALLOC1_ERROR_BAD_VALUE;
		}

		return GRALLOC1_ERROR_UNSUPPORTED;
	}

	return GRALLOC1_ERROR_NONE;
}

/*
 *  Unlocks the Gralloc 1.0 buffer asynchronously.
 *
 * @param device          [in]   Gralloc1 device.
 * @param buffer          [in]   The buffer to unlock.
 * @param outReleaseFence [out]  Refers to an acquire sync fence object.
 *
 * @return 0, when the locking is successful;
 *         Appropriate error, otherwise
 *
 * Note: unlocking a buffer which is not locked results in an unexpected behaviour.
 *       Though it is possible to create a state machine to track the buffer state to
 *       recognize erroneous conditions, it is expected of client to adhere to API
 *       call sequence
 */
static int32_t mali_gralloc1_unlock_async(const gralloc1_device_t * const device,
                                          buffer_handle_t buffer,
                                          int32_t *const outReleaseFence)
{
	mali_gralloc_module *m;
	m = reinterpret_cast<private_module_t *>(device->common.module);

	if (private_handle_t::validate(buffer) < 0)
	{
		return GRALLOC1_ERROR_BAD_HANDLE;
	}

	const int status = mali_gralloc_unlock_async(m, buffer, outReleaseFence);
	if (status != 0)
	{
		if (status == -EINVAL)
		{
			return GRALLOC1_ERROR_BAD_VALUE;
		}

		return GRALLOC1_ERROR_UNSUPPORTED;
	}

	return GRALLOC1_ERROR_NONE;
}

#if PLATFORM_SDK_VERSION >= 26
static int32_t mali_gralloc1_set_layer_count(gralloc1_device_t* device,
                                             gralloc1_buffer_descriptor_t descriptor,
                                             uint32_t layerCount)
{
	int ret = 0;
	ret = mali_gralloc_set_layer_count_internal(descriptor, layerCount);
	GRALLOC_UNUSED(device);
	return ret;
}

static int32_t mali_gralloc1_get_layer_count(gralloc1_device_t* device, buffer_handle_t buffer, uint32_t* outLayerCount)
{
	int ret = 0;
	ret = mali_gralloc_get_layer_count_internal(buffer, outLayerCount);
	GRALLOC_UNUSED(device);
	return ret;
}
#endif

static const mali_gralloc_func mali_gralloc_func_list[] = {
	{ GRALLOC1_FUNCTION_DUMP, (gralloc1_function_pointer_t)mali_gralloc_dump },
	{ GRALLOC1_FUNCTION_CREATE_DESCRIPTOR, (gralloc1_function_pointer_t)mali_gralloc_create_descriptor },
	{ GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR, (gralloc1_function_pointer_t)mali_gralloc_destroy_descriptor },
	{ GRALLOC1_FUNCTION_SET_CONSUMER_USAGE, (gralloc1_function_pointer_t)mali_gralloc_set_consumer_usage },
	{ GRALLOC1_FUNCTION_SET_DIMENSIONS, (gralloc1_function_pointer_t)mali_gralloc_set_dimensions },
	{ GRALLOC1_FUNCTION_SET_FORMAT, (gralloc1_function_pointer_t)mali_gralloc_set_format },
	{ GRALLOC1_FUNCTION_SET_PRODUCER_USAGE, (gralloc1_function_pointer_t)mali_gralloc_set_producer_usage },
	{ GRALLOC1_FUNCTION_GET_BACKING_STORE, (gralloc1_function_pointer_t)mali_gralloc_get_backing_store },
	{ GRALLOC1_FUNCTION_GET_CONSUMER_USAGE, (gralloc1_function_pointer_t)mali_gralloc_get_consumer_usage },
	{ GRALLOC1_FUNCTION_GET_DIMENSIONS, (gralloc1_function_pointer_t)mali_gralloc_get_dimensions },
	{ GRALLOC1_FUNCTION_GET_FORMAT, (gralloc1_function_pointer_t)mali_gralloc_get_format },
	{ GRALLOC1_FUNCTION_GET_PRODUCER_USAGE, (gralloc1_function_pointer_t)mali_gralloc_get_producer_usage },
	{ GRALLOC1_FUNCTION_GET_STRIDE, (gralloc1_function_pointer_t)mali_gralloc_get_stride },
	{ GRALLOC1_FUNCTION_ALLOCATE, (gralloc1_function_pointer_t)mali_gralloc_allocate },
	{ GRALLOC1_FUNCTION_RETAIN, (gralloc1_function_pointer_t)mali_gralloc_retain },
	{ GRALLOC1_FUNCTION_RELEASE, (gralloc1_function_pointer_t)mali_gralloc_release },
	{ GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES, (gralloc1_function_pointer_t)mali_gralloc1_get_num_flex_planes },
	{ GRALLOC1_FUNCTION_LOCK, (gralloc1_function_pointer_t)mali_gralloc1_lock_async },
	{ GRALLOC1_FUNCTION_LOCK_FLEX, (gralloc1_function_pointer_t)mali_gralloc1_lock_flex_async },
	{ GRALLOC1_FUNCTION_UNLOCK, (gralloc1_function_pointer_t)mali_gralloc1_unlock_async },
#if PLATFORM_SDK_VERSION >= 26
	{ GRALLOC1_FUNCTION_SET_LAYER_COUNT, (gralloc1_function_pointer_t)mali_gralloc1_set_layer_count },
	{ GRALLOC1_FUNCTION_GET_LAYER_COUNT, (gralloc1_function_pointer_t)mali_gralloc1_get_layer_count },
#endif

	/* GRALLOC1_FUNCTION_INVALID has to be the last descriptor on the list. */
	{ GRALLOC1_FUNCTION_INVALID, NULL }
};

static void mali_gralloc_getCapabilities(gralloc1_device_t *dev, uint32_t *outCount, int32_t *outCapabilities)
{
	GRALLOC_UNUSED(dev);
#if PLATFORM_SDK_VERSION >= 26
	if (outCount != NULL)
	{
		*outCount = 1;
	}

	if (outCapabilities != NULL)
	{
		*(outCapabilities++) = GRALLOC1_CAPABILITY_LAYERED_BUFFERS;
	}
#else
	GRALLOC_UNUSED(outCapabilities);
	if (outCount != NULL)
	{
		*outCount = 0;
	}
#endif
}

static gralloc1_function_pointer_t mali_gralloc_getFunction(gralloc1_device_t *dev, int32_t descriptor)
{
	GRALLOC_UNUSED(dev);
	gralloc1_function_pointer_t rval = NULL;
	uint32_t pos = 0;

	while (mali_gralloc_func_list[pos].desc != GRALLOC1_FUNCTION_INVALID)
	{
		if (mali_gralloc_func_list[pos].desc == descriptor)
		{
			rval = mali_gralloc_func_list[pos].func;
			break;
		}

		pos++;
	}

	if (rval == NULL)
	{
		rval = mali_gralloc_private_interface_getFunction(descriptor);
	}

	return rval;
}

int mali_gralloc_device_open(hw_module_t const *module, const char *name, hw_device_t **device)
{
	gralloc1_device_t *dev;

	GRALLOC_UNUSED(name);

	dev = new gralloc1_device_t;

	if (NULL == dev)
	{
		return -1;
	}

	/* initialize our state here */
	memset(dev, 0, sizeof(*dev));

	/* initialize the procs */
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = const_cast<hw_module_t *>(module);
	dev->common.close = mali_gralloc_ion_device_close;

	dev->getCapabilities = mali_gralloc_getCapabilities;
	dev->getFunction = mali_gralloc_getFunction;

	*device = &dev->common;

	return 0;
}
