cmake_minimum_required(VERSION 3.20)
project(ghost-layout-fixer LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "ghost-layout-fixer supports Windows only")
endif()

option(BUILD_TESTS "Build unit tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/warnings.cmake)

add_executable(ghost-layout-fixer
    src/main.cpp
    src/app/src/ApplicationService.cpp
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
    add_executable(ghost-layout-fixer-tests
        src/app/src/ApplicationService.cpp
        src/cli/src/CliParser.cpp
        src/core/src/BackupService.cpp
        src/core/src/LayoutFixService.cpp
        src/platform/src/RegistryService.cpp
        src/platform/src/SystemCommandRunner.cpp
        src/platform/src/PrivilegeService.cpp
        src/platform/src/InstalledLanguageService.cpp
        src/report/src/ReportPrinter.cpp
        tests/commands/CommandWorkflowTests.cpp
        tests/core/BackupServiceTests.cpp
        tests/core/LayoutFixServiceTests.cpp
        tests/platform/RegistryServiceTests.cpp
        tests/platform/InstalledLanguageServiceTests.cpp
    )
    target_include_directories(ghost-layout-fixer-tests PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
    glf_apply_warnings(ghost-layout-fixer-tests)
    add_test(NAME ghost-layout-fixer-tests COMMAND ghost-layout-fixer-tests)
endif()
