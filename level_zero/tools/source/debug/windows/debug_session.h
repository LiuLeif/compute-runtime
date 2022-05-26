/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

#include "KmEscape.h"

namespace L0 {

namespace KmHeadersWA {
typedef enum __EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE {
    DBGUMD_RETURN_ESCAPE_SUCCESS = 0, /* Default return type */
    DBGUMD_RETURN_DEBUGGER_ATTACH_DEVICE_BUSY = 1,
    DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED = 2,
    DBGUMD_RETURN_INVALID_ARGS = 3,
    DBGUMD_RETURN_PERMISSION_DENIED = 4,
    DBGUMD_RETURN_NOT_VALID_PROCESS = 5,
    DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED = 6,
    DBGUMD_RETURN_KMD_DEBUG_ERROR = 7,
    DBGUMD_RETURN_INVALID_SESSION_INFO = 8,
    DBGUMD_RETURN_INVALID_EVENT_SEQ_NO = 9,

    DBGUMD_RETURN_TYPE_MAX,

    DBGUMD_RETURN_NOT_IMPLEMENTED = -1
} EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE;
} // namespace KmHeadersWA

struct DebugSessionWindows : DebugSessionImp {
    DebugSessionWindows(const zet_debug_config_t &config, Device *device) : DebugSessionImp(config, device), processId(config.pid) {}

    ze_result_t initialize() override;
    bool closeConnection() override;

    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override;
    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override;
    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override;
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override;

    static ze_result_t translateEscapeReturnStatusToZeResult(uint32_t escapeErrorStatus);

  protected:
    ze_result_t resumeImp(std::vector<ze_device_thread_t> threads, uint32_t deviceIndex) override;
    ze_result_t interruptImp(uint32_t deviceIndex) override;

    ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override;
    ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override;

    ze_result_t readSbaBuffer(EuThread::ThreadId, SbaTrackedAddresses &sbaBuffer) override;

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override;
    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override;
    bool readModuleDebugArea() override;
    void startAsyncThread() override;

    NTSTATUS runEscape(KM_ESCAPE_INFO &escapeInfo);

    NEO::Wddm *wddm = nullptr;
    uint32_t processId = 0;
    uint64_t debugHandle = 0;
};

} // namespace L0