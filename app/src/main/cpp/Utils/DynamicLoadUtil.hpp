//==============================================================================
//
//  Copyright (c) 2019-2022 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================

#pragma once

#include "Logger.hpp"
#include "PAL/DynamicLoading.hpp"
#include "QnnWrapperFunc.hpp"

namespace qnn {
namespace tools {
namespace dynamicloadutil {
enum class StatusCode {
  SUCCESS,
  FAILURE,
  FAIL_LOAD_BACKEND,
  FAIL_LOAD_MODEL,
  FAIL_SYM_FUNCTION,
  FAIL_GET_INTERFACE_PROVIDERS,
  FAIL_LOAD_SYSTEM_LIB,
};

StatusCode getQnnFunctionPointers(std::string backendPath,
                                  std::string modelPath,
                                  func::QnnFunctionPointers* qnnFunctionPointers,
                                  void** backendHandle,
                                  bool loadModelLib,
                                  void** modelHandleRtn);
StatusCode getQnnSystemFunctionPointers(std::string systemLibraryPath,
                                        func::QnnFunctionPointers* qnnFunctionPointers);
}  // namespace dynamicloadutil
}  // namespace tools
}  // namespace qnn
