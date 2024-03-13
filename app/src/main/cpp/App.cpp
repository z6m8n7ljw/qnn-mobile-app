//==============================================================================
//
//  Copyright (c) 2019-2023 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================

#include <inttypes.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include "App.hpp"

using namespace qnn;
using namespace qnn::tools;

void app::split(std::vector<std::string> &splitString,
                       const std::string &tokenizedString,
                       const char separator) {
  splitString.clear();
  std::istringstream tokenizedStringStream(tokenizedString);
  while (!tokenizedStringStream.eof()) {
    std::string value;
    getline(tokenizedStringStream, value, separator);
    if (!value.empty()) {
      splitString.push_back(value);
    }
  }
}

void app::parseInputFilePaths(std::vector<std::string> &inputFilePaths,
                                     std::vector<std::string> &paths,
                                     std::string separator) {
  for (auto &inputInfo : inputFilePaths) {
    auto position = inputInfo.find(separator);
    if (position != std::string::npos) {
      auto path = inputInfo.substr(position + separator.size());
      paths.push_back(path);
    } else {
      paths.push_back(inputInfo);
    }
  }
}

app::ReadInputListsRetType_t app::readInputLists(
    std::vector<std::string> inputFileListPaths) {
  std::vector<std::vector<std::queue<std::string>>> filePathsLists;
  for (auto const &path : inputFileListPaths) {
    bool readSuccess;
    std::vector<std::queue<std::string>> filePathList;
    std::tie(filePathList, readSuccess) = readInputList(path);
    if (!readSuccess) {
      filePathsLists.clear();
      return std::make_tuple(filePathsLists, false);
    }
    filePathsLists.push_back(filePathList);
  }
  return std::make_tuple(filePathsLists, true);
}

app::ReadInputListRetType_t app::readInputList(const std::string inputFileListPath) {
  std::queue<std::string> lines;
  std::ifstream fileListStream(inputFileListPath);
  if (!fileListStream) {
    QNN_ERROR("Failed to open input file: %s", inputFileListPath.c_str());
    std::vector<std::queue<std::string>> result;
    return std::make_tuple(result, false);
  }
  std::string fileLine;
  while (std::getline(fileListStream, fileLine)) {
    if (fileLine.empty()) continue;
    lines.push(fileLine);
  }
  if (!lines.empty() && lines.front().compare(0, 1, "#") == 0) {
    lines.pop();
  }
  std::string separator = ":=";
  std::vector<std::queue<std::string>> filePathsList;
  while (!lines.empty()) {
    std::vector<std::string> paths{};
    std::vector<std::string> inputFilePaths;
    split(inputFilePaths, lines.front(), ' ');
    parseInputFilePaths(inputFilePaths, paths, separator);
    filePathsList.reserve(paths.size());
    for (size_t idx = 0; idx < paths.size(); idx++) {
      if (idx >= filePathsList.size()) {
        filePathsList.push_back(std::queue<std::string>());
      }
      filePathsList[idx].push(paths[idx]);
    }
    lines.pop();
  }
  return std::make_tuple(filePathsList, true);
}

// Default path where the outputs will be stored if outputPath is
// not supplied.
const std::string app::QnnApplication::s_defaultOutputPath = "./output/";

app::QnnApplication::QnnApplication(func::QnnFunctionPointers qnnFunctionPointers,
                                       std::string inputListPaths,
                                       std::string opPackagePaths,
                                       std::string outputPath,
                                       iotensor::OutputDataType outputDataType,
                                       iotensor::InputDataType inputDataType,
                                       bool dumpOutputs)
    : m_qnnFunctionPointers(qnnFunctionPointers),
      m_outputPath(outputPath),
      m_outputDataType(outputDataType),
      m_inputDataType(inputDataType),
      m_dumpOutputs(dumpOutputs),
      m_isBackendInitialized(false),
      m_isContextCreated(false) {
  split(m_inputListPaths, inputListPaths, ',');
  split(m_opPackagePaths, opPackagePaths, ',');
  if (m_outputPath.empty()) {
    m_outputPath = s_defaultOutputPath;
  }
  return;
}

app::QnnApplication::~QnnApplication() {
  // Free Profiling object if it was created
  if (nullptr != m_profileBackendHandle) {
    QNN_DEBUG("Freeing backend profile object.");
    if (QNN_PROFILE_NO_ERROR !=
        m_qnnFunctionPointers.qnnInterface.profileFree(m_profileBackendHandle)) {
      QNN_ERROR("Could not free backend profile handle.");
    }
  }
  // Free context if not already done
  if (m_isContextCreated) {
    QNN_DEBUG("Freeing context");
    if (QNN_CONTEXT_NO_ERROR !=
        m_qnnFunctionPointers.qnnInterface.contextFree(m_context, nullptr)) {
      QNN_ERROR("Could not free context");
    }
  }
  m_isContextCreated = false;
  // Terminate backend
  if (m_isBackendInitialized && nullptr != m_qnnFunctionPointers.qnnInterface.backendFree) {
    QNN_DEBUG("Freeing backend");
    if (QNN_BACKEND_NO_ERROR != m_qnnFunctionPointers.qnnInterface.backendFree(m_backendHandle)) {
      QNN_ERROR("Could not free backend");
    }
  }
  m_isBackendInitialized = false;
  // Terminate logging in the backend
  if (nullptr != m_qnnFunctionPointers.qnnInterface.logFree && nullptr != m_logHandle) {
    if (QNN_SUCCESS != m_qnnFunctionPointers.qnnInterface.logFree(m_logHandle)) {
      QNN_WARN("Unable to terminate logging in the backend.");
    }
  }
  return;
}

// Initialize QnnApplication. Things it does:
//  1. Create output directory
//  2. Read all input list paths provided
//      during creation.
app::StatusCode app::QnnApplication::initialize() {
  // Create Output Directory
  if (m_dumpOutputs && !::pal::FileOp::checkFileExists(m_outputPath) &&
      !pal::Directory::makePath(m_outputPath)) {
    std::cerr << "Could not create output directory: " + m_outputPath;
    return StatusCode::FAILURE;
  }
  // Read Input File List
  bool readSuccess;
  std::tie(m_inputFileLists, readSuccess) = readInputLists(m_inputListPaths);
  if (!readSuccess) {
    std::cerr << "Could not read input lists";
    return StatusCode::FAILURE;
  }
  // initialize logging in the backend
  if (log::isLogInitialized()) {
    auto logCallback = log::getLogCallback();
    auto logLevel    = log::getLogLevel();
    QNN_INFO("Initializing logging in the backend. Callback: [%p], Log Level: [%d]",
             logCallback,
             logLevel);
    if (QNN_SUCCESS !=
        m_qnnFunctionPointers.qnnInterface.logCreate(logCallback, logLevel, &m_logHandle)) {
      QNN_WARN("Unable to initialize logging in the backend.");
    }
  } else {
    QNN_WARN("Logging not available in the backend.");
  }
  return StatusCode::SUCCESS;
}

// Simple method to report error from app to lib.
int32_t app::QnnApplication::reportError(const std::string& err) {
  QNN_ERROR("%s", err.c_str());
  return EXIT_FAILURE;
}

// Initialize a QnnBackend.
app::StatusCode app::QnnApplication::initializeBackend() {
  auto qnnStatus = m_qnnFunctionPointers.qnnInterface.backendCreate(
      m_logHandle, (const QnnBackend_Config_t**)m_backendConfig, &m_backendHandle);
  if (QNN_BACKEND_NO_ERROR != qnnStatus) {
    QNN_ERROR("Could not initialize backend due to error = %d", qnnStatus);
    return StatusCode::FAILURE;
  }
  QNN_INFO("Initialize Backend Returned Status = %d", qnnStatus);
  m_isBackendInitialized = true;
  return StatusCode::SUCCESS;
}

// Register op packages and interface providers supplied during
// object creation. If there are multiple op packages, register
// them sequentially in the order provided.
app::StatusCode app::QnnApplication::registerOpPackages() {
  const size_t pathIdx              = 0;
  const size_t interfaceProviderIdx = 1;
  for (auto const& opPackagePath : m_opPackagePaths) {
    std::vector<std::string> opPackage;
    split(opPackage, opPackagePath, ':');
    QNN_DEBUG("opPackagePath: %s", opPackagePath.c_str());
    const char* target     = nullptr;
    const size_t targetIdx = 2;
    if (opPackage.size() != 2 && opPackage.size() != 3) {
      QNN_ERROR("Malformed opPackageString provided: %s", opPackagePath.c_str());
      return StatusCode::FAILURE;
    }
    if (opPackage.size() == 3) {
      target = (char*)opPackage[targetIdx].c_str();
    }
    if (nullptr == m_qnnFunctionPointers.qnnInterface.backendRegisterOpPackage) {
      QNN_ERROR("backendRegisterOpPackageFnHandle is nullptr.");
      return StatusCode::FAILURE;
    }
    if (QNN_BACKEND_NO_ERROR != m_qnnFunctionPointers.qnnInterface.backendRegisterOpPackage(
                                    m_backendHandle,
                                    (char*)opPackage[pathIdx].c_str(),
                                    (char*)opPackage[interfaceProviderIdx].c_str(),
                                    target)) {
      QNN_ERROR("Could not register Op Package: %s and interface provider: %s",
                opPackage[pathIdx].c_str(),
                opPackage[interfaceProviderIdx].c_str());
      return StatusCode::FAILURE;
    }
    QNN_INFO("Registered Op Package: %s and interface provider: %s",
             opPackage[pathIdx].c_str(),
             opPackage[interfaceProviderIdx].c_str());
  }
  return StatusCode::SUCCESS;
}

// Create a Context in a backend.
app::StatusCode app::QnnApplication::createContext() {
  if (QNN_CONTEXT_NO_ERROR != m_qnnFunctionPointers.qnnInterface.contextCreate(
                                  m_backendHandle,
                                  m_deviceHandle,
                                  (const QnnContext_Config_t**)&m_contextConfig,
                                  &m_context)) {
    QNN_ERROR("Could not create context");
    return StatusCode::FAILURE;
  }
  m_isContextCreated = true;
  return StatusCode::SUCCESS;
}

// Free context after done.
app::StatusCode app::QnnApplication::freeContext() {
  if (QNN_CONTEXT_NO_ERROR !=
      m_qnnFunctionPointers.qnnInterface.contextFree(m_context, m_profileBackendHandle)) {
    QNN_ERROR("Could not free context");
    return StatusCode::FAILURE;
  }
  m_isContextCreated = false;
  return StatusCode::SUCCESS;
}

// Calls composeGraph function in QNN's model.so.
// composeGraphs is supposed to populate graph related
// information in m_graphsInfo and m_graphsCount.
// m_debug is the option supplied to composeGraphs to
// say that all intermediate tensors including output tensors
// are expected to be read by the app.
app::StatusCode app::QnnApplication::composeGraphs() {
  auto returnStatus = StatusCode::SUCCESS;
  if (qnn_wrapper_api::ModelError_t::MODEL_NO_ERROR !=
      m_qnnFunctionPointers.composeGraphsFnHandle(
          m_backendHandle,
          m_qnnFunctionPointers.qnnInterface,
          m_context,
          (const qnn_wrapper_api::GraphConfigInfo_t**)m_graphConfigsInfo,
          m_graphConfigsInfoCount,
          &m_graphsInfo,
          &m_graphsCount,
          false,
          log::getLogCallback(),
          log::getLogLevel())) {
    QNN_ERROR("Failed in composeGraphs()");
    returnStatus = StatusCode::FAILURE;
  }
  return returnStatus;
}

app::StatusCode app::QnnApplication::finalizeGraphs() {
  for (size_t graphIdx = 0; graphIdx < m_graphsCount; graphIdx++) {
    if (QNN_GRAPH_NO_ERROR !=
        m_qnnFunctionPointers.qnnInterface.graphFinalize(
            (*m_graphsInfo)[graphIdx].graph, m_profileBackendHandle, nullptr)) {
      return StatusCode::FAILURE;
    }
  }

  auto returnStatus = StatusCode::SUCCESS;

  return returnStatus;
}

app::StatusCode app::QnnApplication::verifyFailReturnStatus(Qnn_ErrorHandle_t errCode) {
  auto returnStatus = app::StatusCode::FAILURE;
  switch (errCode) {
    case QNN_COMMON_ERROR_SYSTEM_COMMUNICATION:
      returnStatus = app::StatusCode::FAILURE_SYSTEM_COMMUNICATION_ERROR;
      break;
    case QNN_COMMON_ERROR_SYSTEM:
      returnStatus = app::StatusCode::FAILURE_SYSTEM_ERROR;
      break;
    case QNN_COMMON_ERROR_NOT_SUPPORTED:
      returnStatus = app::StatusCode::QNN_FEATURE_UNSUPPORTED;
      break;
    default:
      break;
  }
  return returnStatus;
}

app::StatusCode app::QnnApplication::isDevicePropertySupported() {
  if (nullptr != m_qnnFunctionPointers.qnnInterface.propertyHasCapability) {
    auto qnnStatus =
        m_qnnFunctionPointers.qnnInterface.propertyHasCapability(QNN_PROPERTY_GROUP_DEVICE);
    if (QNN_PROPERTY_NOT_SUPPORTED == qnnStatus) {
      QNN_WARN("Device property is not supported");
    }
    if (QNN_PROPERTY_ERROR_UNKNOWN_KEY == qnnStatus) {
      QNN_ERROR("Device property is not known to backend");
      return StatusCode::FAILURE;
    }
  }
  return StatusCode::SUCCESS;
}

app::StatusCode app::QnnApplication::createDevice() {
  if (nullptr != m_qnnFunctionPointers.qnnInterface.deviceCreate) {
    auto qnnStatus =
        m_qnnFunctionPointers.qnnInterface.deviceCreate(m_logHandle, nullptr, &m_deviceHandle);
    if (QNN_SUCCESS != qnnStatus && QNN_DEVICE_ERROR_UNSUPPORTED_FEATURE != qnnStatus) {
      QNN_ERROR("Failed to create device");
      return verifyFailReturnStatus(qnnStatus);
    }
  }
  return StatusCode::SUCCESS;
}

app::StatusCode app::QnnApplication::freeDevice() {
  if (nullptr != m_qnnFunctionPointers.qnnInterface.deviceFree) {
    auto qnnStatus = m_qnnFunctionPointers.qnnInterface.deviceFree(m_deviceHandle);
    if (QNN_SUCCESS != qnnStatus && QNN_DEVICE_ERROR_UNSUPPORTED_FEATURE != qnnStatus) {
      QNN_ERROR("Failed to free device");
      return verifyFailReturnStatus(qnnStatus);
    }
  }
  return StatusCode::SUCCESS;
}

// executeGraphs() that is currently used by qnn-sample-app's main.cpp.
// This function runs all the graphs present in model.so by reading
// inputs from input_list based files and writes output to .raw files.
app::StatusCode app::QnnApplication::executeGraphs() {
  auto returnStatus = StatusCode::SUCCESS;
  for (size_t graphIdx = 0; graphIdx < m_graphsCount; graphIdx++) {
    QNN_DEBUG("Starting execution for graphIdx: %d", graphIdx);
    if (graphIdx >= m_inputFileLists.size()) {
      QNN_ERROR("No Inputs available for: %d", graphIdx);
      returnStatus = StatusCode::FAILURE;
      break;
    }
    Qnn_Tensor_t* inputs  = nullptr;
    Qnn_Tensor_t* outputs = nullptr;
    if (iotensor::StatusCode::SUCCESS !=
        m_ioTensor.setupInputAndOutputTensors(&inputs, &outputs, (*m_graphsInfo)[graphIdx])) {
      QNN_ERROR("Error in setting up Input and output Tensors for graphIdx: %d", graphIdx);
      returnStatus = StatusCode::FAILURE;
      break;
    }
    auto inputFileList = m_inputFileLists[graphIdx];
    auto graphInfo     = (*m_graphsInfo)[graphIdx];
    if (!inputFileList.empty()) {
      size_t totalCount = inputFileList[0].size();
      while (!inputFileList[0].empty()) {
        size_t startIdx = (totalCount - inputFileList[0].size());
        if (iotensor::StatusCode::SUCCESS !=
            m_ioTensor.populateInputTensors(
                graphIdx, inputFileList, inputs, graphInfo, m_inputDataType)) {
          returnStatus = StatusCode::FAILURE;
        }
        if (StatusCode::SUCCESS == returnStatus) {
          QNN_DEBUG("Successfully populated input tensors for graphIdx: %d", graphIdx);
          Qnn_ErrorHandle_t executeStatus = QNN_GRAPH_NO_ERROR;
          executeStatus =
              m_qnnFunctionPointers.qnnInterface.graphExecute(graphInfo.graph,
                                                              inputs,
                                                              graphInfo.numInputTensors,
                                                              outputs,
                                                              graphInfo.numOutputTensors,
                                                              m_profileBackendHandle,
                                                              nullptr);
          if (QNN_GRAPH_NO_ERROR != executeStatus) {
            returnStatus = StatusCode::FAILURE;
          }
          if (StatusCode::SUCCESS == returnStatus) {
            QNN_DEBUG("Successfully executed graphIdx: %d ", graphIdx);
            if (iotensor::StatusCode::SUCCESS !=
                m_ioTensor.writeOutputTensors(graphIdx,
                                              startIdx,
                                              graphInfo.graphName,
                                              outputs,
                                              graphInfo.numOutputTensors,
                                              m_outputDataType,
                                              m_graphsCount,
                                              m_outputPath)) {
              returnStatus = StatusCode::FAILURE;
            }
          }
        }
        if (StatusCode::SUCCESS != returnStatus) {
          QNN_ERROR("Execution of Graph: %d failed!", graphIdx);
          break;
        }
      }
    }
    m_ioTensor.tearDownInputAndOutputTensors(
        inputs, outputs, graphInfo.numInputTensors, graphInfo.numOutputTensors);
    inputs  = nullptr;
    outputs = nullptr;
    if (StatusCode::SUCCESS != returnStatus) {
      break;
    }
  }

  qnn_wrapper_api::freeGraphsInfo(&m_graphsInfo, m_graphsCount);
  m_graphsInfo = nullptr;
  return returnStatus;
}
