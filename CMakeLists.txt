cmake_minimum_required(VERSION 3.16)
project(adiboupk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
