//==============================================================================
//
//  Copyright (c) 2019-2023 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

#include "AppUtils.hpp"

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

app::ProfilingLevel app::parseProfilingLevel(std::string profilingLevelString) {
  std::transform(profilingLevelString.begin(),
                 profilingLevelString.end(),
                 profilingLevelString.begin(),
                 ::tolower);
  ProfilingLevel parsedProfilingLevel = ProfilingLevel::INVALID;
  if (profilingLevelString == "off") {
    parsedProfilingLevel = ProfilingLevel::OFF;
  } else if (profilingLevelString == "basic") {
    parsedProfilingLevel = ProfilingLevel::BASIC;
  } else if (profilingLevelString == "detailed") {
    parsedProfilingLevel = ProfilingLevel::DETAILED;
  }
  return parsedProfilingLevel;
}

QnnLog_Level_t app::parseLogLevel(std::string logLevelString) {
  QNN_FUNCTION_ENTRY_LOG;
  std::transform(logLevelString.begin(), logLevelString.end(), logLevelString.begin(), ::tolower);
  QnnLog_Level_t parsedLogLevel = QNN_LOG_LEVEL_MAX;
  if (logLevelString == "error") {
    parsedLogLevel = QNN_LOG_LEVEL_ERROR;
  } else if (logLevelString == "warn") {
    parsedLogLevel = QNN_LOG_LEVEL_WARN;
  } else if (logLevelString == "info") {
    parsedLogLevel = QNN_LOG_LEVEL_INFO;
  } else if (logLevelString == "verbose") {
    parsedLogLevel = QNN_LOG_LEVEL_VERBOSE;
  } else if (logLevelString == "debug") {
    parsedLogLevel = QNN_LOG_LEVEL_DEBUG;
  }
  QNN_FUNCTION_EXIT_LOG;
  return parsedLogLevel;
}
