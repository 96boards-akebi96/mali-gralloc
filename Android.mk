# 
# Copyright (C) 2016 ARM Limited. All rights reserved.
# 
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

TOP_LOCAL_PATH := $(call my-dir)
MALI_GRALLOC_API_TESTS?=0

# Build gralloc
include $(TOP_LOCAL_PATH)/src/Android.mk

# Build gralloc api tests
ifeq ($(MALI_GRALLOC_API_TESTS), 1)
include $(TOP_LOCAL_PATH)/api_tests/Android.mk
endif
