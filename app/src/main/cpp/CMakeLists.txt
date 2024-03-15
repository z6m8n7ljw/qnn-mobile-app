cmake_minimum_required(VERSION 3.22.1)

# Define project name
project(qnn-mobile-app)

# Set the C++ standard and optimization level
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Werror -fvisibility=hidden")

# Include directories
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        include/QNN
        Log
        PAL/include
        Utils
        Wrapper
)

# Add source files
file(GLOB_RECURSE SRC_FILES
        "*.cpp"
        "Log/*.cpp"
        "PAL/src/linux/*.cpp"
        "PAL/src/common/*.cpp"
        "Utils/*.cpp"
        "Wrapper/*.cpp"
        )

add_executable(qnn-mobile-app ${SRC_FILES})

# Link libraries
target_link_libraries(qnn-mobile-app
        GLESv2
        EGL
        dl
        log
        android
)