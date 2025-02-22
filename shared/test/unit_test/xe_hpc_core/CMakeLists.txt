#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  set(NEO_SHARED_tests_xe_hpc_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_2_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe_hpc_core_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_xe_hpc_core_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_xe_hpc_core.cpp
  )

  set(IGDRCL_SRCS_tests_xe_hpc_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpc_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpc_core_excludes})

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    list(APPEND NEO_SHARED_tests_xe_hpc_core
         ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_lib_tests_xe_hpc_core.cpp
    )
  endif()

  target_sources(${TARGET_NAME} PRIVATE
                 ${IGDRCL_SRCS_tests_xe_hpc_core_excludes}
                 ${NEO_SHARED_tests_xe_hpc_core}
  )

  add_subdirectories()
endif()
