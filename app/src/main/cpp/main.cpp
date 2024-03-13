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

#include "Logger.hpp"
#include "PAL/DynamicLoading.hpp"
#include "PAL/GetOpt.hpp"
#include "DynamicLoadUtil.hpp"
//#include "AppUtils.hpp"
#include "App.hpp"

int main(int argc, char** argv) {
    void* sg_backendHandle{nullptr};
    void* sg_modelHandle{nullptr};

    using namespace qnn::tools;

    enum OPTIONS {
        OPT_MODEL           = 0,
        OPT_BACKEND         = 1,
        OPT_INPUT_LIST      = 2,
        OPT_OUTPUT_DIR      = 3,
    };

    // Create the command line options
    static struct pal::Option s_longOptions[] = {
            {"model", pal::required_argument, NULL, OPT_MODEL},
            {"backend", pal::required_argument, NULL, OPT_BACKEND},
            {"input_list", pal::required_argument, NULL, OPT_INPUT_LIST},
            {"output_dir", pal::required_argument, NULL, OPT_OUTPUT_DIR},
            {NULL, 0, NULL, 0}};

    // Command line parsing loop
    int longIndex = 0;
    int opt       = 0;
    std::string modelPath;
    std::string backEndPath;
    std::string inputListPaths;
    std::string outputPath;
    std::string opPackagePaths;
    iotensor::OutputDataType parsedOutputDataType   = iotensor::OutputDataType::FLOAT_ONLY;
    iotensor::InputDataType parsedInputDataType     = iotensor::InputDataType::FLOAT;

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
                                                              true, 
                                                              &sg_modelHandle);
    if (dynamicloadutil::StatusCode::SUCCESS != statusCode) {
        if (dynamicloadutil::StatusCode::FAIL_LOAD_BACKEND == statusCode) {
            std::cerr << "Error initializing QNN Function Pointers: could not load backend: " + backEndPath;
            return EXIT_FAILURE;
        } else if (dynamicloadutil::StatusCode::FAIL_LOAD_MODEL == statusCode) {
            std::cerr << "Error initializing QNN Function Pointers: could not load model: " + modelPath;
            return EXIT_FAILURE;
        } else {
            std::cerr << "Error initializing QNN Function Pointers";
            return EXIT_FAILURE;
        }
    }

    if (!qnn::log::initializeLogging()) {
        std::cerr << "ERROR: Unable to initialize logging!\n";
        return EXIT_FAILURE;
    }

    {
        std::unique_ptr<app::QnnApplication> app(new app::QnnApplication(qnnFunctionPointers, 
                                                                                inputListPaths, 
                                                                                opPackagePaths,
                                                                                outputPath,
                                                                                parsedOutputDataType, 
                                                                                parsedInputDataType,
                                                                                true));

        if (nullptr == app) {
            return EXIT_FAILURE;
        }

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

        if (app::StatusCode::SUCCESS != app->registerOpPackages()) {
            return app->reportError("Register Op Packages failure");
        }

        if (app::StatusCode::SUCCESS != app->createContext()) {
            return app->reportError("Context Creation failure");
        }
        if (app::StatusCode::SUCCESS != app->composeGraphs()) {
            return app->reportError("Graph Prepare failure");
        }
        if (app::StatusCode::SUCCESS != app->finalizeGraphs()) {
            return app->reportError("Graph Finalize failure");
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
