//==============================================================================
//
//    Copyright (c) 2019-2022 Qualcomm Technologies, Inc.
//    All Rights Reserved.
//    Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================

#include <iostream>
#include <memory>
#include <string>

#include "BuildId.hpp"
#include "Logger.hpp"
#include "PAL/DynamicLoading.hpp"
#include "PAL/GetOpt.hpp"
#include "DynamicLoadUtil.hpp"
#include "AppUtils.hpp"
#include "App.hpp"

static void* sg_backendHandle{nullptr};
static void* sg_modelHandle{nullptr};

namespace qnn {
namespace tools {
namespace app {

std::unique_ptr<app::QnnSampleApp> processCommandLine(int argc, char** argv, bool& loadFromCachedBinary) {
    enum OPTIONS {
        OPT_MODEL           = 0,
        OPT_BACKEND         = 1,
        OPT_INPUT_LIST      = 2,
        OPT_OUTPUT_DIR      = 3,
        OPT_LOG_LEVEL       = 4,
        OPT_PROFILING_LEVEL = 5
    };

    // Create the command line options
    static struct pal::Option s_longOptions[] = {
            {"model", pal::required_argument, NULL, OPT_MODEL},
            {"backend", pal::required_argument, NULL, OPT_BACKEND},
            {"input_list", pal::required_argument, NULL, OPT_INPUT_LIST},
            {"output_dir", pal::required_argument, NULL, OPT_OUTPUT_DIR},
            {"profiling_level", pal::required_argument, NULL, OPT_PROFILING_LEVEL},
            {"log_level", pal::required_argument, NULL, OPT_LOG_LEVEL},
            {NULL, 0, NULL, 0}};

    // Command line parsing loop
    int longIndex = 0;
    int opt       = 0;
    std::string modelPath;
    std::string backEndPath;
    std::string inputListPaths;
    bool debug = false;
    std::string outputPath;
    std::string opPackagePaths;
    iotensor::OutputDataType parsedOutputDataType   = iotensor::OutputDataType::FLOAT_ONLY;
    iotensor::InputDataType parsedInputDataType     = iotensor::InputDataType::FLOAT;
    app::ProfilingLevel parsedProfilingLevel = ProfilingLevel::OFF;
    bool dumpOutputs = true;
    std::string cachedBinaryPath;
    std::string saveBinaryName;
    QnnLog_Level_t logLevel{QNN_LOG_LEVEL_ERROR};

    while ((opt = pal::getOptLongOnly(argc, argv, "", s_longOptions, &longIndex)) != -1) {
        switch (opt) {
            case OPT_MODEL:
                modelPath = pal::g_optArg;
                break;
            case OPT_BACKEND:
                backEndPath = pal::g_optArg;
                break;
            case OPT_INPUT_LIST:
                inputListPaths = pal::g_optArg;
                break;
            case OPT_OUTPUT_DIR:
                outputPath = pal::g_optArg;
                break;
            case OPT_PROFILING_LEVEL:
                parsedProfilingLevel = app::parseProfilingLevel(pal::g_optArg);
                if (parsedProfilingLevel == app::ProfilingLevel::INVALID) {
                    std::cerr << "Invalid profiling level." << "\n";
                }
                break;
            case OPT_LOG_LEVEL:
                logLevel = app::parseLogLevel(pal::g_optArg);
                if (logLevel != QNN_LOG_LEVEL_MAX) {
                    if (!log::setLogLevel(logLevel)) {
                        std::cerr << "Unable to set log level." << "\n";
                    }
                }
                break;
            default:
                std::cerr << "ERROR: Invalid argument passed: " << argv[pal::g_optInd - 1]
                          << "\nPlease check the Arguments section in the description below.\n";
                std::exit(EXIT_FAILURE);
        }
    }

    if (modelPath.empty()) {
        std::cerr << "Missing option: --model\n" << "\n";
    }

    if (backEndPath.empty()) {
        std::cerr << "Missing option: --backend\n" << "\n";
    }

    if (inputListPaths.empty()) {
        std::cerr << "Missing option: --input_list\n" << "\n";
    }

    QNN_INFO("Model: %s", modelPath.c_str());
    QNN_INFO("Backend: %s", backEndPath.c_str());

    func::QnnFunctionPointers qnnFunctionPointers;
    // Load backend and model .so and validate all the required function symbols are resolved
    auto statusCode = dynamicloadutil::getQnnFunctionPointers(backEndPath, 
                                                              modelPath, 
                                                              &qnnFunctionPointers, 
                                                              &sg_backendHandle, 
                                                              !loadFromCachedBinary, 
                                                              &sg_modelHandle);
    if (dynamicloadutil::StatusCode::SUCCESS != statusCode) {
        if (dynamicloadutil::StatusCode::FAIL_LOAD_BACKEND == statusCode) {
            exitWithMessage(
                    "Error initializing QNN Function Pointers: could not load backend: " + backEndPath,
                    EXIT_FAILURE);
        } else if (dynamicloadutil::StatusCode::FAIL_LOAD_MODEL == statusCode) {
            exitWithMessage(
                    "Error initializing QNN Function Pointers: could not load model: " + modelPath,
                    EXIT_FAILURE);
        } else {
            exitWithMessage("Error initializing QNN Function Pointers", EXIT_FAILURE);
        }
    }

    std::unique_ptr<app::QnnSampleApp> app(new app::QnnSampleApp(qnnFunctionPointers, 
                                                                               inputListPaths, 
                                                                               opPackagePaths, 
                                                                               sg_backendHandle, 
                                                                               outputPath, 
                                                                               debug, 
                                                                               parsedOutputDataType, 
                                                                               parsedInputDataType, 
                                                                               parsedProfilingLevel, 
                                                                               dumpOutputs, 
                                                                               cachedBinaryPath, 
                                                                               saveBinaryName));
    return app;
}

}    // namespace sample_app
}    // namespace tools
}    // namespace qnn

int main(int argc, char** argv) {
    using namespace qnn::tools;

    if (!qnn::log::initializeLogging()) {
        std::cerr << "ERROR: Unable to initialize logging!\n";
        return EXIT_FAILURE;
    }

    {
        bool loadFromCachedBinary{false};
        std::unique_ptr<app::QnnSampleApp> app =
                app::processCommandLine(argc, argv, loadFromCachedBinary);

        if (nullptr == app) {
            return EXIT_FAILURE;
        }

        QNN_INFO("qnn-sample-app build version: %s", qnn::tools::getBuildId().c_str());
        QNN_INFO("Backend build version: %s", app->getBackendBuildId().c_str());

        if (app::StatusCode::SUCCESS != app->initialize()) {
            return app->reportError("Initialization failure");
        }

        if (app::StatusCode::SUCCESS != app->initializeBackend()) {
            return app->reportError("Backend Initialization failure");
        }

        auto devicePropertySupportStatus = app->isDevicePropertySupported();
        if (app::StatusCode::FAILURE != devicePropertySupportStatus) {
            auto createDeviceStatus = app->createDevice();
            if (app::StatusCode::SUCCESS != createDeviceStatus) {
                return app->reportError("Device Creation failure");
            }
        }

        if (app::StatusCode::SUCCESS != app->initializeProfiling()) {
            return app->reportError("Profiling Initialization failure");
        }

        if (app::StatusCode::SUCCESS != app->registerOpPackages()) {
            return app->reportError("Register Op Packages failure");
        }

        if (!loadFromCachedBinary) {
            if (app::StatusCode::SUCCESS != app->createContext()) {
                return app->reportError("Context Creation failure");
            }
            if (app::StatusCode::SUCCESS != app->composeGraphs()) {
                return app->reportError("Graph Prepare failure");
            }
            if (app::StatusCode::SUCCESS != app->finalizeGraphs()) {
                return app->reportError("Graph Finalize failure");
            }
        } else {
            if (app::StatusCode::SUCCESS != app->createFromBinary()) {
                return app->reportError("Create From Binary failure");
            }
        }

        if (app::StatusCode::SUCCESS != app->executeGraphs()) {
            return app->reportError("Graph Execution failure");
        }

        if (app::StatusCode::SUCCESS != app->freeContext()) {
            return app->reportError("Context Free failure");
        }

        if (app::StatusCode::FAILURE != devicePropertySupportStatus) {
            auto freeDeviceStatus = app->freeDevice();
            if (app::StatusCode::SUCCESS != freeDeviceStatus) {
                return app->reportError("Device Free failure");
            }
        }
    }

    if (sg_backendHandle) {
        pal::dynamicloading::dlClose(sg_backendHandle);
    }
    if (sg_modelHandle) {
        pal::dynamicloading::dlClose(sg_modelHandle);
    }

    return EXIT_SUCCESS;
}
