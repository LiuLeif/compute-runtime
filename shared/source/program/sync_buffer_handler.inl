/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

template <typename KernelT>
void NEO::SyncBufferHandler::prepareForEnqueue(size_t workGroupsCount, KernelT &kernel) {
    auto requiredSize = workGroupsCount;
    std::lock_guard<std::mutex> guard(this->mutex);

    bool isCurrentBufferFull = (usedBufferSize + requiredSize > bufferSize);
    if (isCurrentBufferFull) {
        memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
        allocateNewBuffer();
        usedBufferSize = 0;
    }

    kernel.patchSyncBuffer(device, graphicsAllocation, usedBufferSize);

    usedBufferSize += requiredSize;
}