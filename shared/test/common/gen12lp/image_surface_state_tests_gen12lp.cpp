/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using namespace NEO;

using ImageSurfaceStateTestsGen12LP = ImageSurfaceStateTests;

GEN12LPTEST_F(ImageSurfaceStateTestsGen12LP, givenGmmWithMediaCompressedWhenSetFlagsForMediaCompressionThenAuxiliarySurfaceNoneIsSetAndMemoryCompressionEnable) {
    auto size = sizeof(typename TGLLPFamily::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename TGLLPFamily::RENDER_SURFACE_STATE *>(surfaceState.get());
    castSurfaceState->setAuxiliarySurfaceMode(TGLLPFamily::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

    mockGmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = false;
    EncodeSurfaceState<FamilyType>::setFlagsForMediaCompression(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), TGLLPFamily::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    EXPECT_EQ(castSurfaceState->getMemoryCompressionEnable(), false);
    mockGmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    EncodeSurfaceState<FamilyType>::setFlagsForMediaCompression(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), TGLLPFamily::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(castSurfaceState->getMemoryCompressionEnable(), true);
}

GEN12LPTEST_F(ImageSurfaceStateTestsGen12LP, givenGmmWhenSetClearColorParamsThenClearValueAddressEnable) {
    auto size = sizeof(typename TGLLPFamily::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename TGLLPFamily::RENDER_SURFACE_STATE *>(surfaceState.get());

    mockGmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = true;
    EncodeSurfaceState<TGLLPFamily>::setClearColorParams(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getClearValueAddressEnable(), true);
}

GEN12LPTEST_F(ImageSurfaceStateTestsGen12LP, givenGmmWithMediaCompressedWhenSetMipTailStartLodThenMipTailStartLodIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    setMipTailStartLod<FamilyType>(castSurfaceState, nullptr);

    EXPECT_EQ(castSurfaceState->getMipTailStartLod(), 0u);

    setMipTailStartLod<FamilyType>(castSurfaceState, mockGmm.get());

    EXPECT_EQ(castSurfaceState->getMipTailStartLod(), mockGmm->gmmResourceInfo->getMipTailStartLodSurfaceState());
}
