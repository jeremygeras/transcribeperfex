cmake_minimum_required(VERSION 3.20.0)

project(core VERSION 0.0.0)

include(ExternalProject)
ExternalProject_Add(
    aws-sdk-cpp
    PREFIX ${CMAKE_BINARY_DIR}/aws-sdk-cpp
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp
    GIT_TAG cca7d9210b373f847ff9c73900a6832ffdd3d231
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    #GIT_SUBMODULES TODO specify submodules to speed up checkout
    CMAKE_ARGS "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/output" "-DENABLE_TESTING=OFF" "-DBUILD_ONLY=transcribestreaming,polly,s3"
    LIST_SEPARATOR ,
    BUILD_COMMAND cmake --build . --config=Release
    INSTALL_COMMAND cmake --install . --config=Release
)

ExternalProject_Add(
    cli
    PREFIX ${CMAKE_BINARY_DIR}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cli
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/output" "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    BUILD_COMMAND cmake --build . --config=RelWithDebInfo
    INSTALL_COMMAND cmake --install . --config=RelWithDebInfo
    DEPENDS aws-sdk-cpp
)
