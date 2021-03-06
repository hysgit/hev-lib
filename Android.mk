# Copyright (C) 2013 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := libhev-lib
LOCAL_SRC_FILES := \
	src/hev-async-queue.c \
	src/hev-event-loop.c \
	src/hev-event-source-fds.c \
	src/hev-event-source-idle.c \
	src/hev-event-source-signal.c \
	src/hev-event-source-timeout.c \
	src/hev-event-source.c \
	src/hev-list.c \
	src/hev-memory-allocator.c \
	src/hev-memory-allocator-slice.c \
	src/hev-queue.c \
	src/hev-ring-buffer.c \
	src/hev-slist.c \
	src/hev-hash-table.c
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -mfpu=neon
endif
include $(BUILD_STATIC_LIBRARY)

