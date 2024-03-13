//==============================================================================
//
//  Copyright (c) 2019-2022 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================
#pragma once

#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "Logger.hpp"
#include "PAL/Directory.hpp"
#include "PAL/FileOp.hpp"
#include "PAL/Path.hpp"
#include "PAL/StringOp.hpp"
#include "QnnTypeMacros.hpp"

namespace qnn {
namespace tools {
namespace app {

enum class ProfilingLevel { OFF, BASIC, DETAILED, INVALID };

using ReadInputListRetType_t = std::tuple<std::vector<std::queue<std::string>>, bool>;

ReadInputListRetType_t readInputList(std::string inputFileListPath);

using ReadInputListsRetType_t = std::tuple<std::vector<std::vector<std::queue<std::string>>>, bool>;

ReadInputListsRetType_t readInputLists(std::vector<std::string> inputFileListPath);

ProfilingLevel parseProfilingLevel(std::string profilingLevelString);

void parseInputFilePaths(std::vector<std::string> &inputFilePaths,
                         std::vector<std::string> &paths,
                         std::string separator);

void split(std::vector<std::string> &splitString,
           const std::string &tokenizedString,
           const char separator);

QnnLog_Level_t parseLogLevel(std::string logLevelString);

void inline exitWithMessage(std::string &&msg, int code) {
  std::cerr << msg << std::endl;
  std::exit(code);
}

}  // namespace app
}  // namespace tools
}  // namespace qnn