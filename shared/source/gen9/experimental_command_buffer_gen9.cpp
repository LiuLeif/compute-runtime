/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/experimental_command_buffer.inl"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"

namespace NEO {
typedef SKLFamily GfxFamily;

template void ExperimentalCommandBuffer::injectBufferStart<GfxFamily>(LinearStream &parentStream, size_t cmdBufferOffset);
template size_t ExperimentalCommandBuffer::getRequiredInjectionSize<GfxFamily>() noexcept;

template size_t ExperimentalCommandBuffer::programExperimentalCommandBuffer<GfxFamily>();
template size_t ExperimentalCommandBuffer::getTotalExperimentalSize<GfxFamily>() noexcept;

template void ExperimentalCommandBuffer::addTimeStampPipeControl<GfxFamily>();
template size_t ExperimentalCommandBuffer::getTimeStampPipeControlSize<GfxFamily>() noexcept;

template void ExperimentalCommandBuffer::addExperimentalCommands<GfxFamily>();
template size_t ExperimentalCommandBuffer::getExperimentalCommandsSize<GfxFamily>() noexcept;
} // namespace NEO
