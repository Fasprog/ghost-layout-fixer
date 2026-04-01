cmake_minimum_required(VERSION 3.20)
project(ghost-layout-fixer LANGUAGES CXX)

option(BUILD_TESTS "Build unit tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/warnings.cmake)

add_executable(ghost-layout-fixer
    src/main.cpp
    src/cli/src/CliParser.cpp
    src/core/src/BackupService.cpp
    src/core/src/LayoutFixService.cpp
    src/platform/src/RegistryService.cpp
    src/platform/src/SystemCommandRunner.cpp
    src/platform/src/PrivilegeService.cpp
    src/platform/src/InstalledLanguageService.cpp
    src/report/src/ReportPrinter.cpp
    src/common/src/Logger.cpp
)

target_include_directories(ghost-layout-fixer PRIVATE
    ${CMAKE_SOURCE_DIR}
)

glf_apply_warnings(ghost-layout-fixer)

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "BUILD_TESTS is ON, test targets will be added in later stages")
endif()
