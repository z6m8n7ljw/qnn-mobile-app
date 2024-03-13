//==============================================================================
//
//  Copyright (c) 2020, 2022 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================
#pragma once

#include <memory>
#include <queue>

#include "QnnBackend.h"
#include "QnnCommon.h"
#include "QnnContext.h"
#include "QnnGraph.h"
#include "QnnProperty.h"
#include "QnnTensor.h"
#include "QnnTypes.h"
#include "System/QnnSystemInterface.h"

#include "DataUtil.hpp"
#include "Logger.hpp"
#include "PAL/Directory.hpp"
#include "PAL/FileOp.hpp"
#include "PAL/Path.hpp"
#include "PAL/StringOp.hpp"
#include "QnnTypeMacros.hpp"
#include "QnnWrapperUtils.hpp"

namespace qnn {
namespace tools {
namespace iotensor {

enum class StatusCode { SUCCESS, FAILURE };
enum class OutputDataType { FLOAT_ONLY, NATIVE_ONLY, FLOAT_AND_NATIVE, INVALID };
enum class InputDataType { FLOAT, NATIVE, INVALID };

OutputDataType parseOutputDataType(std::string dataTypeString);
InputDataType parseInputDataType(std::string dataTypeString);

class IOTensor {
 public:
  IOTensor() : m_batchSize(1), m_numFilesPopulated(0) {}

  StatusCode setupInputAndOutputTensors(Qnn_Tensor_t **inputs,
                                        Qnn_Tensor_t **outputs,
                                        qnn_wrapper_api::GraphInfo_t graphInfo);

  StatusCode writeOutputTensors(uint32_t graphIdx,
                                size_t startIdx,
                                char *graphName,
                                Qnn_Tensor_t *outputs,
                                uint32_t numOutputs,
                                OutputDataType outputDatatype,
                                uint32_t graphsCount,
                                std::string outputPath);

  StatusCode populateInputTensors(uint32_t graphIdx,
                                  std::vector<std::queue<std::string>> &filePathsQueue,
                                  Qnn_Tensor_t *inputs,
                                  qnn_wrapper_api::GraphInfo_t graphInfo,
                                  iotensor::InputDataType inputDataType);

  StatusCode populateInputTensors(uint32_t graphIdx,
                                  std::vector<uint8_t *> inputBuffers,
                                  Qnn_Tensor_t *inputs,
                                  qnn_wrapper_api::GraphInfo_t graphInfo,
                                  InputDataType inputDataType);

  StatusCode tearDownInputAndOutputTensors(Qnn_Tensor_t *inputs,
                                           Qnn_Tensor_t *outputs,
                                           size_t numInputTensors,
                                           size_t numOutputTensors);

  bool copyMetadataToGraphsInfo(const QnnSystemContext_BinaryInfo_t *binaryInfo,
                                qnn_wrapper_api::GraphInfo_t **&graphsInfo,
                                uint32_t &graphsCount);

  bool copyGraphsInfo(const QnnSystemContext_GraphInfo_t *graphsInput,
                      const uint32_t numGraphs,
                      qnn_wrapper_api::GraphInfo_t **&graphsInfo);

  bool copyGraphsInfoV1(const QnnSystemContext_GraphInfoV1_t *graphInfoSrc,
                        qnn_wrapper_api::GraphInfo_t *graphInfoDst);

  bool copyTensorsInfo(const Qnn_Tensor_t *tensorsInfoSrc,
                      Qnn_Tensor_t *&tensorWrappers,
                      uint32_t tensorsCount);

  bool deepCopyQnnTensorInfo(Qnn_Tensor_t *dst, const Qnn_Tensor_t *src);

 private:
  size_t m_batchSize;
  size_t m_numFilesPopulated;

  StatusCode populateInputTensor(std::queue<std::string> &filePaths,
                                 Qnn_Tensor_t *input,
                                 InputDataType inputDataType);

  StatusCode populateInputTensor(uint8_t *buffer, Qnn_Tensor_t *input, InputDataType inputDataType);

  StatusCode readDataAndAllocateBuffer(std::queue<std::string> &filePaths,
                                       std::vector<size_t> dims,
                                       Qnn_DataType_t dataType,
                                       uint8_t **bufferToCopy);

  template <typename T>
  StatusCode allocateBuffer(T **buffer, size_t &elementCount);

  StatusCode convertToFloat(float **out, Qnn_Tensor_t *output);

  StatusCode convertAndWriteOutputTensorInFloat(Qnn_Tensor_t *output,
                                                std::vector<std::string> outputPaths,
                                                std::string fileName);

  StatusCode writeOutputTensor(Qnn_Tensor_t *output,
                               std::vector<std::string> outputPaths,
                               std::string fileName);

  StatusCode allocateAndCopyBuffer(uint8_t **buffer, Qnn_Tensor_t *tensor);

  StatusCode tearDownTensors(Qnn_Tensor_t *tensors, uint32_t tensorCount);

  StatusCode allocateBuffer(uint8_t **buffer, std::vector<size_t> dims, Qnn_DataType_t dataType);

  StatusCode copyFromFloatToNative(float *floatBuffer, Qnn_Tensor_t *tensor);

  StatusCode setupTensors(Qnn_Tensor_t **tensors, uint32_t tensorCount, Qnn_Tensor_t *tensorsInfo);

  StatusCode fillDims(std::vector<size_t> &dims, uint32_t *inDimensions, uint32_t rank);
};
}  // namespace iotensor
}  // namespace tools
}  // namespace qnn