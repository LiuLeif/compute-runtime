/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_allocator_multi_device_fixture.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"

using namespace NEO;

void MemoryAllocatorMultiDeviceSystemSpecificFixture::SetUp(ExecutionEnvironment &executionEnvironment) {
    auto memoryManager = static_cast<TestedDrmMemoryManager *>(executionEnvironment.memoryManager.get());
    auto bufferObject = new (std::nothrow) BufferObject(&memoryManager->getDrm(0u), 3, 0, 10, MemoryManager::maxOsContextCount);
    memoryManager->pushSharedBufferObject(bufferObject);
}

void MemoryAllocatorMultiDeviceSystemSpecificFixture::TearDown(ExecutionEnvironment &executionEnvironment) {
    auto memoryManager = static_cast<TestedDrmMemoryManager *>(executionEnvironment.memoryManager.get());
    auto bufferObject = memoryManager->sharingBufferObjects.back();
    memoryManager->eraseSharedBufferObject(bufferObject);
    delete bufferObject;
}
