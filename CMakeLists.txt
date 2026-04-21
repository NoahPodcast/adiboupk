cmake_minimum_required(VERSION 3.16)
project(adiboupk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Version from git tag ---
# Uses git describe to get version like "v1.2.0" or "v1.2.0-3-gabcdef".
# Falls back to project(VERSION) if git is unavailable or no tags exist.
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0 AND GIT_VERSION MATCHES "^v?[0-9]")
        # Strip leading 'v' if present
        string(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_VERSION}")
        set(ADIBOUPK_VERSION "${GIT_VERSION}")
    else()
        set(ADIBOUPK_VERSION "${PROJECT_VERSION}")
    endif()
else()
    set(ADIBOUPK_VERSION "${PROJECT_VERSION}")
endif()
message(STATUS "adiboupk version: ${ADIBOUPK_VERSION}")

# Sources
set(SOURCES
    src/main.cpp
    src/cli.cpp
    src/config.cpp
    src/discovery.cpp
    src/venv.cpp
    src/installer.cpp
    src/auditor.cpp
    src/runner.cpp
    src/platform_common.cpp
    src/utils.cpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND SOURCES src/platform_windows.cpp)
else()
    list(APPEND SOURCES src/platform_linux.cpp)
endif()

add_executable(adiboupk ${SOURCES})

target_include_directories(adiboupk PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_definitions(adiboupk PRIVATE ADIBOUPK_VERSION="${ADIBOUPK_VERSION}")

# Static linking for portable binary
if(WIN32)
    target_compile_definitions(adiboupk PRIVATE UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(adiboupk PRIVATE /MT)
    else()
        target_link_options(adiboupk PRIVATE -static-libgcc -static-libstdc++)
    endif()
else()
    target_link_options(adiboupk PRIVATE -static-libgcc -static-libstdc++)
endif()

# Library target for tests (everything except main.cpp)
set(LIB_SOURCES
    src/cli.cpp
    src/config.cpp
    src/discovery.cpp
    src/venv.cpp
    src/installer.cpp
    src/auditor.cpp
    src/runner.cpp
    src/platform_common.cpp
    src/utils.cpp
)
if(WIN32)
    list(APPEND LIB_SOURCES src/platform_windows.cpp)
else()
    list(APPEND LIB_SOURCES src/platform_linux.cpp)
endif()

add_library(adiboupk_lib STATIC ${LIB_SOURCES})
target_include_directories(adiboupk_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_definitions(adiboupk_lib PUBLIC ADIBOUPK_VERSION="${ADIBOUPK_VERSION}")

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_discovery.cpp
        tests/test_auditor.cpp
        tests/test_runner.cpp
        tests/test_config.cpp
        tests/test_utils.cpp
        tests/test_platform.cpp
    )

    add_executable(adiboupk_tests ${TEST_SOURCES})
    target_link_libraries(adiboupk_tests PRIVATE adiboupk_lib)
    target_include_directories(adiboupk_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party
    )
    add_test(NAME unit_tests COMMAND adiboupk_tests)
endif()

# Install
install(TARGETS adiboupk DESTINATION bin)
