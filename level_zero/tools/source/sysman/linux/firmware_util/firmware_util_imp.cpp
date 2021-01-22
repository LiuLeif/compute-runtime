/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_imp.h"

namespace L0 {
const std::string FirmwareUtilImp::fwUtilLibraryFile = "libigsc.so.0";
const std::string FirmwareUtilImp::fwDeviceInitByDevice = "igsc_device_init_by_device_info";
const std::string FirmwareUtilImp::fwDeviceGetDeviceInfo = "igsc_device_get_device_info";
const std::string FirmwareUtilImp::fwDeviceFwVersion = "igsc_device_fw_version";
const std::string FirmwareUtilImp::fwDeviceIteratorCreate = "igsc_device_iterator_create";
const std::string FirmwareUtilImp::fwDeviceIteratorNext = "igsc_device_iterator_next";
const std::string FirmwareUtilImp::fwDeviceIteratorDestroy = "igsc_device_iterator_destroy";
const std::string FirmwareUtilImp::fwDeviceFwUpdate = "igsc_device_fw_update";

template <class T>
bool FirmwareUtilImp::getSymbolAddr(const std::string name, T &proc) {
    void *addr = libraryHandle->getProcAddress(name);
    proc = reinterpret_cast<T>(addr);
    return nullptr != proc;
}

bool FirmwareUtilImp::loadEntryPoints() {
    bool ok = getSymbolAddr(fwDeviceInitByDevice, deviceInitByDevice);
    ok = ok && getSymbolAddr(fwDeviceGetDeviceInfo, deviceGetDeviceInfo);
    ok = ok && getSymbolAddr(fwDeviceFwVersion, deviceGetFwVersion);
    ok = ok && getSymbolAddr(fwDeviceIteratorCreate, deviceIteratorCreate);
    ok = ok && getSymbolAddr(fwDeviceIteratorNext, deviceItreatorNext);
    ok = ok && getSymbolAddr(fwDeviceIteratorDestroy, deviceItreatorDestroy);
    ok = ok && getSymbolAddr(fwDeviceFwUpdate, deviceFwUpdate);
    return ok;
}

static void progressFunc(uint32_t done, uint32_t total, void *ctx) {
    uint32_t percent = (done * 100) / total;
    PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stdout, "Progess: %d/%d:%d/%\n", done, total, percent);
}

ze_result_t FirmwareUtilImp::getFirstDevice(igsc_device_info *info) {
    igsc_device_iterator *iter;
    int ret = deviceIteratorCreate(&iter);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    info->name[0] = '\0';
    ret = deviceItreatorNext(iter, info);
    if (ret == IGSC_SUCCESS) {
        deviceItreatorDestroy(iter);
        return ZE_RESULT_SUCCESS;
    }
    deviceItreatorDestroy(iter);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t FirmwareUtilImp::fwDeviceInit() {
    char *devicePath = nullptr;
    int ret;
    igsc_device_info info;
    ze_result_t result = getFirstDevice(&info);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    devicePath = strdup(info.name);
    ret = deviceInitByDevice(&fwDeviceHandle, devicePath);
    if (ret != 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    fwDevicePath = *devicePath;
    free(devicePath);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwGetVersion(std::string &fwVersion) {
    igsc_fw_version deviceFwVersion;
    memset(&deviceFwVersion, 0, sizeof(deviceFwVersion));
    int ret = deviceGetFwVersion(&fwDeviceHandle, &deviceFwVersion);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    fwVersion.append(deviceFwVersion.project);
    fwVersion.append("_");
    fwVersion.append(std::to_string(deviceFwVersion.hotfix));
    fwVersion.append(".");
    fwVersion.append(std::to_string(deviceFwVersion.build));
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwFlashGSC(void *pImage, uint32_t size) {
    int ret = deviceFwUpdate(&fwDeviceHandle, static_cast<const uint8_t *>(pImage), size, progressFunc, nullptr);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}
FirmwareUtilImp::FirmwareUtilImp(){};

FirmwareUtilImp::~FirmwareUtilImp() {
    if (nullptr != libraryHandle) {
        delete libraryHandle;
        libraryHandle = nullptr;
    }
};

FirmwareUtil *FirmwareUtil::create() {
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp();
    UNRECOVERABLE_IF(nullptr == pFwUtilImp);
    pFwUtilImp->libraryHandle = NEO::OsLibrary::load(pFwUtilImp->fwUtilLibraryFile);
    if (pFwUtilImp->libraryHandle == nullptr || pFwUtilImp->loadEntryPoints() == false) {
        if (nullptr != pFwUtilImp->libraryHandle) {
            delete pFwUtilImp->libraryHandle;
            pFwUtilImp->libraryHandle = nullptr;
        }
        delete pFwUtilImp;
        return nullptr;
    }

    return static_cast<FirmwareUtil *>(pFwUtilImp);
}
} // namespace L0
