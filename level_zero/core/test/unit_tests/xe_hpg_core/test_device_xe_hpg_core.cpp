/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using DeviceQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(DeviceQueueGroupTest, givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned, IsXeHpgCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);
}

HWTEST2_F(DeviceQueueGroupTest, givenNoBlitterSupportAndCCSThenTwoQueueGroupsAreReturned, IsXeHpgCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 2u);
}

HWTEST2_F(DeviceQueueGroupTest, givenBlitterSupportAndCCSThenThreeQueueGroupsAreReturned, IsXeHpgCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Compute)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, hwInfo.featureTable.ftrBcsInfo.count());
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
        }
    }
}

class DeviceCopyQueueGroupFixture : public DeviceFixture {
  public:
    void SetUp() {
        DebugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::SetUp();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

using DeviceCopyQueueGroupTest = Test<DeviceCopyQueueGroupFixture>;

HWTEST2_F(DeviceCopyQueueGroupTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsXeHpgCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_NE(i, static_cast<uint32_t>(NEO::EngineGroupType::Copy));
    }
}

using TestDeviceXeHpgCore = Test<DeviceFixture>;
HWTEST2_F(TestDeviceXeHpgCore, givenReturnedDevicePropertiesThenExpectedPropertyFlagsSet, IsXeHpgCore) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_INTEGRATED, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
    } else {
        EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
    }
}
} // namespace ult
} // namespace L0