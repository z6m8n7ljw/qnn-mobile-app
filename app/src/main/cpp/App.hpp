//==============================================================================
//
//  Copyright (c) 2020-2022 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================
#pragma once

#include <memory>
#include <queue>

#include "IOTensor.hpp"

#include "DataUtil.hpp"
#include "Logger.hpp"
#include "PAL/Directory.hpp"
#include "PAL/FileOp.hpp"
#include "PAL/Path.hpp"
#include "PAL/StringOp.hpp"
#include "QnnTypeMacros.hpp"
#include "QnnWrapperFunc.hpp"
#include "QnnWrapperUtils.hpp"

namespace qnn {
namespace tools {
namespace app {

using ReadInputListRetType_t = std::tuple<std::vector<std::queue<std::string>>, bool>;

ReadInputListRetType_t readInputList(std::string inputFileListPath);

using ReadInputListsRetType_t = std::tuple<std::vector<std::vector<std::queue<std::string>>>, bool>;

ReadInputListsRetType_t readInputLists(std::vector<std::string> inputFileListPath);

void parseInputFilePaths(std::vector<std::string> &inputFilePaths,
                         std::vector<std::string> &paths,
                         std::string separator);

void split(std::vector<std::string> &splitString,
           const std::string &tokenizedString,
           const char separator);

enum class StatusCode {
  SUCCESS,
  FAILURE,
  FAILURE_INPUT_LIST_EXHAUSTED,
  FAILURE_SYSTEM_ERROR,
  FAILURE_SYSTEM_COMMUNICATION_ERROR,
  QNN_FEATURE_UNSUPPORTED
};

class QnnApplication {
 public:
  QnnApplication(func::QnnFunctionPointers qnnFunctionPointers,
               std::string inputListPaths,
               std::string opPackagePaths,
               std::string outputPath                  = s_defaultOutputPath,
               iotensor::OutputDataType outputDataType = iotensor::OutputDataType::FLOAT_ONLY,
               iotensor::InputDataType inputDataType   = iotensor::InputDataType::FLOAT,
               bool dumpOutputs                        = false);

  // @brief Print a message to STDERR then return a nonzero
  //  exit status.
  int32_t reportError(const std::string &err);

  StatusCode initialize();

  StatusCode initializeBackend();

  StatusCode createContext();

  StatusCode composeGraphs();

  StatusCode finalizeGraphs();

  StatusCode executeGraphs();

  StatusCode registerOpPackages();

  StatusCode freeContext();

  StatusCode isDevicePropertySupported();

  StatusCode createDevice();

  StatusCode freeDevice();

  StatusCode verifyFailReturnStatus(Qnn_ErrorHandle_t errCode);

  virtual ~QnnApplication();

 private:
  static const std::string s_defaultOutputPath;

  func::QnnFunctionPointers m_qnnFunctionPointers;
  std::vector<std::string> m_inputListPaths;
  std::vector<std::vector<std::queue<std::string>>> m_inputFileLists;
  std::vector<std::string> m_opPackagePaths;
  std::string m_outputPath;
  QnnBackend_Config_t **m_backendConfig = nullptr;
  Qnn_ContextHandle_t m_context         = nullptr;
  QnnContext_Config_t **m_contextConfig = nullptr;
  iotensor::OutputDataType m_outputDataType;
  iotensor::InputDataType m_inputDataType;
  bool m_dumpOutputs;
  qnn_wrapper_api::GraphInfo_t **m_graphsInfo;
  uint32_t m_graphsCount;
  iotensor::IOTensor m_ioTensor;
  bool m_isBackendInitialized;
  bool m_isContextCreated;
  Qnn_ProfileHandle_t m_profileBackendHandle              = nullptr;
  qnn_wrapper_api::GraphConfigInfo_t **m_graphConfigsInfo = nullptr;
  uint32_t m_graphConfigsInfoCount;
  Qnn_LogHandle_t m_logHandle         = nullptr;
  Qnn_BackendHandle_t m_backendHandle = nullptr;
  Qnn_DeviceHandle_t m_deviceHandle   = nullptr;
};
}  // namespace app
}  // namespace tools
}  // namespace qnn
